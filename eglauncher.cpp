/*
 * Copyright © 2016-2019 Octopull Limited.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "eglauncher.h"
#include "egfullscreenclient.h"
#include "printer.h"

#include <linux/input.h>
#include <xkbcommon/xkbcommon.h>

#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>
#include <thread>

#include <vector>
#include <mir/geometry/size.h>

using namespace miral;

namespace
{
using file_list = std::vector<boost::filesystem::path>;

auto list_desktop_files() -> file_list;

auto ends_with_desktop(std::string const &full_string) -> bool
{
    static const std::string desktop{".desktop"};

    if (full_string.length() < desktop.length())
        return false;

    return !full_string.compare(full_string.length() - desktop.length(), desktop.length(), desktop);
}

void scan_directory_for_desktop_files(file_list& list, boost::filesystem::path const& path)
try
{
    for (boost::filesystem::directory_iterator i(path), end; i != end; ++i)
    {
        if (is_directory(*i))
        {
            scan_directory_for_desktop_files(list, *i);
        }
        else if (ends_with_desktop(i->path().filename().string()))
        {
            list.push_back(i->path());
        }
    }
}
catch (std::exception const&){}

auto scan_for_desktop_files(std::vector<boost::filesystem::path> const& paths) -> file_list
{
    file_list list;

    for (auto const& path : paths)
    {
        if (is_directory(path))
        {
            scan_directory_for_desktop_files(list, path);
        }
    }

    return list;
}

auto search_paths(char const* search_path) -> file_list
{
    file_list paths;

    for (char const* start = search_path; char const* end = strchr(start, ':'); start = end+1)
    {
        if (start == end) continue;

        if (strncmp(start, "~/", 2) != 0)
        {
            paths.push_back(boost::filesystem::path{start, end});
        }
        else
        {
            paths.push_back(boost::filesystem::path{getenv("HOME")} / boost::filesystem::path{start + 2, end});
        }
    }
    return paths;
}

struct app_details
{
    std::string name;
    std::string exec;
    std::string icon;
    std::string title;
};

auto load_details() -> std::vector<app_details>
{
    static std::string const categories_key{"Categories="};
    static std::string const name_key{"Name="};
    static std::string const exec_key{"Exec="};
    static std::string const icon_key{"Icon="};

    auto const desktop_listing = list_desktop_files();

    std::vector<app_details> details;

    for (auto const& desktop : desktop_listing)
    {
        boost::filesystem::ifstream in(desktop);

        std::string line;

        std::string categories;
        std::string name;
        std::string exec;
        std::string icon;

        auto in_desktop_entry = false;

        while (std::getline(in, line))
        {
            if (line == "[Desktop Entry]")
            {
                in_desktop_entry = true;
            }
            else if (line.find("[Desktop Action") == 0)
            {
                in_desktop_entry = false;
            }
            else if (in_desktop_entry)
            {
                if (line.find(categories_key) == 0)
                    categories = line.substr(categories_key.length()) + ";";
                else if (line.find(name_key) == 0 && name.empty())
                    name = line.substr(name_key.length());
                else if (line.find(exec_key) == 0)
                    exec = line.substr(exec_key.length());
                else if (line.find(icon_key) == 0)
                    icon = line.substr(icon_key.length());
            }
        }

        auto app = exec;
        auto ws = app.find('%');
        if (ws != std::string::npos)
        {
            // TODO handle exec variables:
            // https://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables
            app = app.substr(0, ws);    // For now ignore the rest of the Exec value
        }

        auto sl = app.rfind('/');
        if (sl != std::string::npos)
            app.erase(0, sl+1);

        auto sp = app.find(' ');
        if (sp != std::string::npos)
            app.erase(sp, app.size());

        auto title = name + " [" + app + ']';

        if (!name.empty() && !exec.empty())
            details.push_back(app_details{name, exec, icon, title});
    }

    std::sort(begin(details), end(details),
        [](app_details const& lhs, app_details const& rhs)
            { return lhs.title < rhs.title; });

    details.erase(
        std::unique(begin(details), end(details),
            [](app_details const& lhs, app_details const& rhs)
                { return lhs.title == rhs.title; }),
        end(details));

    std::string::size_type max_length = 0;

    for (auto const& detail : details)
        max_length = std::max(max_length, detail.title.size());

    for (auto& detail : details)
    {
        auto const pl = (max_length - detail.title.size()) / 2;
        detail.title.insert(0, pl, ' ');
        detail.title.insert(detail.title.size(), pl, ' ');
    }

    return details;
}

auto list_desktop_files() -> file_list
{
    std::string search_path;
    if (auto const* start = getenv("XDG_DATA_DIRS"))
    {
        for (auto const* end = start;
            *end && ((end = strchr(start, ':')) || (end = strchr(start, '\0')));
            start = end+1)
        {
            if (start == end) continue;

            if (strncmp(start, "~/", 2) != 0)
            {
                search_path += std::string{start, end} + "/applications:";
            }
            else if (auto const home = getenv("HOME"))
            {
                search_path += home + std::string{start + 1, end} + "/applications:";
            }
        }
    }
    else
    {
        search_path = "/usr/local/share/applications:/usr/share/applications:/var/lib/snapd/desktop/applications:";
    }

    auto const paths = search_paths(search_path.c_str());
    return scan_for_desktop_files(paths);

}
}

struct egmde::Launcher::Self : egmde::FullscreenClient
{
    Self(wl_display* display, ExternalClientLauncher& external_client_launcher);

    void draw_screen(SurfaceInfo& info) const override;
    void show_screen(SurfaceInfo& info) const;
    void clear_screen(SurfaceInfo& info) const;

    void start();
    void run_app(std::string app) const;

private:
    void prev_app();
    void next_app();
    void run_app();

    void keyboard_key(wl_keyboard* keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) override;
    void keyboard_leave(wl_keyboard* keyboard, uint32_t serial, wl_surface* surface) override;

    void pointer_motion(wl_pointer* pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y) override;
    void pointer_button(wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) override;
    void pointer_enter(wl_pointer* pointer, uint32_t serial, wl_surface* surface, wl_fixed_t x, wl_fixed_t y) override;

    void touch_down(
        wl_touch* touch, uint32_t serial, uint32_t time, wl_surface* surface, int32_t id, wl_fixed_t x,
        wl_fixed_t y) override;

private:

    ExternalClientLauncher& external_client_launcher;

    int pointer_y = 0;
    int height = 0;

    std::vector<app_details> const apps = load_details();

    std::mutex mutable mutex;
    std::vector<app_details>::const_iterator current_app{apps.begin()};
    std::atomic<bool> running{false};
    std::atomic<Output const*> mutable showing{nullptr};
};

egmde::Launcher::Launcher(miral::ExternalClientLauncher& external_client_launcher) :
    external_client_launcher{external_client_launcher}
{
}

void egmde::Launcher::stop()
{
    if (auto ss = self.lock())
    {
        ss->stop();
        std::lock_guard<decltype(mutex)> lock{mutex};
        ss.reset();
    }
}

void egmde::Launcher::show()
{
    if (auto ss = self.lock())
    {
        ss->start();
    }
}

void egmde::Launcher::operator()(wl_display* display)
{
    auto client = std::make_shared<Self>(display, external_client_launcher);
    self = client;
    client->run(display);

    // Possibly need to wait for stop() to release the client.
    // (This would be less ugly with a ref-counted wrapper for wl_display* in the miral API)
    std::lock_guard<decltype(mutex)> lock{mutex};
}

void egmde::Launcher::run_app(std::string app) const
{
    if (auto ss = self.lock())
    {
        ss->run_app(app);
    }
}

void egmde::Launcher::Self::start()
{
    if (!running.exchange(true))
    {
        auto const new_terminal = std::find_if(
            begin(apps), end(apps),
            [](app_details const& app)
                { return app.name == "Terminal"; });

        if (new_terminal != end(apps))
            current_app = new_terminal;

        showing = nullptr;
        for_each_surface([this](auto& info) { draw_screen(info);});
    }
}

void egmde::Launcher::Self::keyboard_key(wl_keyboard* /*keyboard*/, uint32_t /*serial*/, uint32_t /*time*/, uint32_t key, uint32_t state)
{
    if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
    {
        switch (auto const keysym = xkb_state_key_get_one_sym(keyboard_state(), key+8))
        {
        case XKB_KEY_Right:
        case XKB_KEY_Down:
            next_app();
            break;

        case XKB_KEY_Left:
        case XKB_KEY_Up:
            prev_app();
            break;

        case XKB_KEY_Return:
        case XKB_KEY_space:
            run_app();
            break;

        case XKB_KEY_Escape:
            running = false;
            for_each_surface([this](auto& info) { draw_screen(info); });
            break;

        default:
        {
            uint32_t utf32 = xkb_keysym_to_utf32(keysym);

            if (isalnum(utf32))
            {
                char const text[] = {static_cast<char>(toupper(utf32)), '\0'};

                auto p = current_app + 1;
                auto end = apps.end();

                if (p == end || text < current_app->name.substr(0,1))
                {
                    p = apps.begin();
                    end = current_app;
                }

                while (text > p->name.substr(0,1) && p != apps.end())
                    ++p;

                if (p != apps.end())
                {
                    current_app = p;
                    for_each_surface([this](auto& info) { draw_screen(info); });
                }
            }
        }
        }
    }
}

void egmde::Launcher::Self::pointer_motion(wl_pointer* pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
    pointer_y = wl_fixed_to_int(y);
    FullscreenClient::pointer_motion(pointer, time, x, y);
}

void egmde::Launcher::Self::pointer_button(
    wl_pointer* pointer,
    uint32_t serial,
    uint32_t time,
    uint32_t button,
    uint32_t state)
{
    if (BTN_LEFT == button &&
        WL_POINTER_BUTTON_STATE_PRESSED == state)
    {
        if (pointer_y < height/3)
            prev_app();
        else if (pointer_y > (2*height)/3)
            next_app();
        else
            run_app();
    }

    FullscreenClient::pointer_button(pointer, serial, time, button, state);
}

void egmde::Launcher::Self::pointer_enter(
    wl_pointer* pointer,
    uint32_t serial,
    wl_surface* surface,
    wl_fixed_t x,
    wl_fixed_t y)
{
    pointer_y = wl_fixed_to_int(y);

    for_each_surface([&, this](SurfaceInfo& info)
        {
            if (surface == info.surface)
                height = info.output->height;
        });

    FullscreenClient::pointer_enter(pointer, serial, surface, x, y);
}

void egmde::Launcher::Self::touch_down(
    wl_touch* touch, uint32_t serial, uint32_t time, wl_surface* surface, int32_t id, wl_fixed_t x, wl_fixed_t y)
{
    auto const touch_y = wl_fixed_to_int(y);
    int height = -1;

    for_each_surface([&height, surface](SurfaceInfo& info)
         {
             if (surface == info.surface)
                height = info.output->height;
         });

    if (height >= 0)
    {
        if (touch_y < height/3)
            prev_app();
        else if (touch_y > (2*height)/3)
            next_app();
        else
            run_app();
    }

    FullscreenClient::touch_down(touch, serial, time, surface, id, x, y);
}

void egmde::Launcher::Self::run_app()
{
    auto app = current_app->exec;

    run_app(app);

    running = false;
    for_each_surface([this](auto& info) { draw_screen(info); });
}

void egmde::Launcher::Self::run_app(std::string app) const
{
    setenv("NO_AT_BRIDGE", "1", 1);
    unsetenv("DISPLAY");

    auto ws = app.find('%');
    if (ws != std::string::npos)
    {
        // TODO handle exec variables:
        // https://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables
        if (app[ws-1] == ' ')
            --ws;
        app = app.substr(0, ws);    // For now ignore the rest of the Exec value
    }

    if (app == "qterminal --drop")
        app = "qterminal";

    if (app == "gnome-terminal" && boost::filesystem::exists("/usr/bin/gnome-terminal.real"))
        app = "gnome-terminal --disable-factory";

    static char const* launch_prefix = getenv("EGMDE_LAUNCH_PREFIX");

    std::vector<std::string> command;

    char const* start = nullptr;
    char const* end = nullptr;

    if (launch_prefix)
    {
        for (start = launch_prefix; (end = strchr(start, ' ')); start = end+1)
        {
            if (start != end)
                command.emplace_back(start, end);
        }

        command.emplace_back(start);
    }

    for (start = app.c_str(); (end = strchr(start, ' ')); start = end+1)
    {
        if (start != end)
            command.emplace_back(start, end);
    }

    command.emplace_back(start);

    this->external_client_launcher.launch(command);
}

void egmde::Launcher::Self::next_app()
{
    if (++current_app == apps.end())
        current_app = apps.begin();

    for_each_surface([this](auto& info) { draw_screen(info); });
}

void egmde::Launcher::Self::prev_app()
{
    if (current_app == apps.begin())
        current_app = apps.end();

    --current_app;

    for_each_surface([this](auto& info) { draw_screen(info); });
}

egmde::Launcher::Self::Self(wl_display* display, ExternalClientLauncher& external_client_launcher) :
    FullscreenClient{display},
    external_client_launcher{external_client_launcher}
{
    wl_display_roundtrip(display);
    wl_display_roundtrip(display);
}

void egmde::Launcher::Self::draw_screen(SurfaceInfo& info) const
{
    if (running)
    {
        show_screen(info);
    }
    else
    {
        clear_screen(info);
    }
}

void egmde::Launcher::Self::show_screen(SurfaceInfo& info) const
{

    Output const* active_output = showing.load();

    if (active_output && active_output != info.output)
        return;

    if (!showing.compare_exchange_strong(active_output, info.output))
        return;

    bool const rotated = info.output->transform & WL_OUTPUT_TRANSFORM_90;
    auto const width = rotated ? info.output->height : info.output->width;
    auto const height = rotated ? info.output->width : info.output->height;

    if (width <= 0 || height <= 0)
        return;

    auto const stride = 4 * width;

    if (!info.surface)
    {
        info.surface = wl_compositor_create_surface(compositor);
    }

    if (!info.shell_surface)
    {
        info.shell_surface = wl_shell_get_shell_surface(shell, info.surface);
        wl_shell_surface_set_fullscreen(
            info.shell_surface,
            WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
            0,
            info.output->output);
    }

    if (info.buffer)
    {
        wl_buffer_destroy(info.buffer);
    }

    info.buffer = wl_shm_pool_create_buffer(
        make_shm_pool(stride * height, &info.content_area).get(),
        0,
        width, height, stride,
        WL_SHM_FORMAT_ARGB8888);

    static uint8_t const pattern[4] = {0x1f, 0x1f, 0x1f, 0xaf};

    auto const content_area = reinterpret_cast<unsigned char*>(info.content_area);
    auto row = content_area;

    for (int j = 0; j != height; ++j)
    {
        for (int i = 0; i < width; i++)
            memcpy(row + 4 * i, pattern, 4);
        row += stride;
    }

    // One day we'll use the icon file

    static Printer printer;
    printer.print(width, height, content_area, current_app->title);
    printer.footer(
        width, height, content_area,
        {"<Enter> = start app | Arrow keys = change app | initial letter = change app | <Esc> = cancel", ""});

    wl_surface_attach(info.surface, info.buffer, 0, 0);
    wl_surface_commit(info.surface);
}

void egmde::Launcher::Self::clear_screen(SurfaceInfo& info) const
{
    info.clear_window();
}

void egmde::Launcher::Self::keyboard_leave(wl_keyboard* /*keyboard*/, uint32_t /*serial*/, wl_surface* /*surface*/)
{
    running = false;
    for_each_surface([this](auto& info) { draw_screen(info); });
}
