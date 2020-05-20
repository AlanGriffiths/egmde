/*
 * Copyright © 2016-2020 Octopull Limited.
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

#include "open_desktop_entry.h"

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
    std::string desktop_file;
};

auto unescape(std::string const& in) -> std::string
{
    std::string result;
    bool escape = false;

    for (auto c : in)
    {
        if (!(escape = (!escape && c == '\\')))
            result += c;
    }

    return result;
}

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
                    exec = unescape(line.substr(exec_key.length()));
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

        if (!name.empty() && !exec.empty())
            details.push_back(app_details{name, exec, icon, name, desktop.string()});
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

    static auto const title_size_limit = 30;
    for (auto& detail : details)
    {
        if (detail.title.size() > title_size_limit)
        {
            detail.title = detail.title.substr(0, title_size_limit-3) + "...";
        }
        max_length = std::max(max_length, detail.title.size());
    }

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
    Self(wl_display* display, ExternalClientLauncher& external_client_launcher, miral::MirRunner const& runner);

    void draw_screen(SurfaceInfo& info) const override;
    void show_screen(SurfaceInfo& info) const;
    void clear_screen(SurfaceInfo& info) const;

    void start();

    void run_app(std::string app, Mode mode) const;
private:
    void prev_app();
    void next_app();
    void run_app(Mode mode = Mode::wayland);

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
    miral::MirRunner const& runner;

    int pointer_y = 0;
    int height = 0;

    std::vector<app_details> const apps = load_details();

    std::mutex mutable mutex;
    std::vector<app_details>::const_iterator current_app{apps.begin()};
    std::atomic<bool> running{false};
    std::atomic<Output const*> mutable showing{nullptr};
};

egmde::Launcher::Launcher(miral::ExternalClientLauncher& external_client_launcher, miral::MirRunner const& runner) :
    external_client_launcher{external_client_launcher}, runner{runner}
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
    auto client = std::make_shared<Self>(display, external_client_launcher, runner);
    self = client;
    client->run(display);

    // Possibly need to wait for stop() to release the client.
    // (This would be less ugly with a ref-counted wrapper for wl_display* in the miral API)
    std::lock_guard<decltype(mutex)> lock{mutex};
}

void egmde::Launcher::run_app(std::string app, Mode mode) const
{
    if (auto ss = self.lock())
    {
        ss->run_app(app, mode);
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
        for_each_surface([this](auto& info) { this->draw_screen(info);});
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

        case XKB_KEY_BackSpace:
            run_app(Mode::x11);
            break;

        case XKB_KEY_F11:
            run_app(Mode::wayland_debug);
            break;

        case XKB_KEY_F12:
            run_app(Mode::x11_debug);
            break;

        case XKB_KEY_Escape:
            running = false;
            for_each_surface([this](auto& info) { this->draw_screen(info); });
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
                    for_each_surface([this](auto& info) { this->draw_screen(info); });
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

void egmde::Launcher::Self::run_app(Mode mode)
{
    auto app = current_app->exec;

    run_app(app, mode);

    running = false;
    for_each_surface([this](auto& info) { this->draw_screen(info); });
}

void egmde::Launcher::Self::run_app(std::string app, Mode mode) const
{
    if (getenv("EGMDE_SNAPCRAFT_LAUNCH") == nullptr)
    {
        auto ws = app.find('%');
        if (ws != std::string::npos)
        {
            // TODO handle exec variables:
            // https://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables
            if (app[ws-1] == ' ')
                --ws;
            app = app.substr(0, ws);    // For now ignore the rest of the Exec value
        }

        std::vector<std::string> command;

        char const* start = nullptr;
        char const* end = nullptr;

        static char const* launch_prefix = getenv("EGMDE_LAUNCH_PREFIX");
        if (launch_prefix)
        {
            for (start = launch_prefix; (end = strchr(start, ' ')); start = end+1)
            {
                if (start != end)
                    command.emplace_back(start, end);
            }

            command.emplace_back(start);
        }

        switch (mode)
        {
        case Mode::wayland_debug:
        case Mode::x11_debug:
            command.emplace_back("gnome-terminal");
            if (boost::filesystem::exists("/usr/bin/gnome-terminal.real"))
                command.emplace_back("--disable-factory");
            command.emplace_back("--");
            command.emplace_back("bash");
            command.emplace_back("-c");
            command.emplace_back(app + ";read -p \"Press any key to continue... \" -n1 -s");
            break;

        case Mode::wayland:
        case Mode::x11:
            {
                std::string token;
                char in_quote = '\0';
                bool escaping = false;

                auto push_token = [&]()
                    {
                        if (!token.empty())
                        {
                            command.push_back(std::move(token));
                            token.clear();
                        }
                    };

                for (auto c : app)
                {
                    if (escaping)
                    {
                        // end escape
                        escaping = false;
                        token += c;
                        continue;
                    }

                    switch (c)
                    {
                    case '\\':
                        // start escape
                        escaping = true;
                        continue;

                    case '\'':
                    case '\"':
                        if (in_quote == '\0')
                        {
                            // start quoted sequence
                            in_quote = c;
                            continue;
                        }
                        else if (c == in_quote)
                        {
                            // end quoted sequence
                            in_quote = '\0';
                            continue;
                        }
                        else
                        {
                            break;
                        }

                    default:
                        break;
                    }

                    if (!isspace(c) || in_quote)
                    {
                        token += c;
                    }
                    else
                    {
                        push_token();
                    }
                }

                push_token();
            }
            break;
        }

        switch (mode)
        {
        case Mode::wayland:
        case Mode::wayland_debug:
            external_client_launcher.launch(command);
            break;

        case Mode::x11:
        case Mode::x11_debug:
            external_client_launcher.launch_using_x11(command);
            break;
        }
    }
    else
    {
        std::vector<std::string> env{
            "XDG_SESSION_DESKTOP=mir",
            "XDG_SESSION_TYPE=wayland"};

        if (auto const& wayland_display = runner.wayland_display())
        {
            env.push_back("WAYLAND_DISPLAY=" + wayland_display.value());
        }

        if (auto const& x11_display = runner.x11_display())
        {
            env.push_back("DISPLAY=" + x11_display.value());
        }

        open_desktop_entry(current_app->desktop_file, env);
    }
}

void egmde::Launcher::Self::next_app()
{
    if (++current_app == apps.end())
        current_app = apps.begin();

    for_each_surface([this](auto& info) { this->draw_screen(info); });
}

void egmde::Launcher::Self::prev_app()
{
    if (current_app == apps.begin())
        current_app = apps.end();

    --current_app;

    for_each_surface([this](auto& info) { this->draw_screen(info); });
}

egmde::Launcher::Self::Self(wl_display* display, ExternalClientLauncher& external_client_launcher, MirRunner const& runner) :
    FullscreenClient{display},
    external_client_launcher{external_client_launcher},
    runner{runner}
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

    auto const prev = (current_app == apps.begin() ? apps.end() : current_app) - 1;
    auto const next = current_app == apps.end()-1 ? apps.begin() : current_app + 1;

    static Printer printer;
    printer.print(width, height, content_area, {prev->title,  current_app->title, next->title});
    auto const help =
        "<Enter> = start app | "
        "<BkSp> = start using X11 | "
        "Arrows (or initial letter) = change app | <Esc> = cancel";
    printer.footer(width, height, content_area, {help, ""});

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
    for_each_surface([this](auto& info) { this->draw_screen(info); });
}
