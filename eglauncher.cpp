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

auto scan_for_desktop_files(std::vector<boost::filesystem::path> const& paths) -> file_list
{
    file_list list;

    for (auto const& path : paths)
    {
        if (is_directory(path))
        try
        {
            for (boost::filesystem::directory_iterator i(path), end; i != end; ++i)
            {
                if (!is_directory(*i) && ends_with_desktop(i->path().filename().string()))
                {
                    list.push_back(i->path());
                }
            }
        }
        catch (std::exception const&){}
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

        while (std::getline(in, line))
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
    std::string search_path{"~/.local/share/applications:/usr/share/applications:/var/lib/snapd/desktop/applications"};
    if (char const* desktop_path = getenv("EGMDE_DESKTOP_PATH"))
        search_path = desktop_path;
    // search_paths relies on a ":" sentinal value
    search_path +=  ":";
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

    void launch();

//    void stop();
//
//private:
//    static void lifecycle_event_callback(MirConnection* /*connection*/, MirLifecycleState state, void* context);
//    static void window_event_callback(MirWindow* window, MirEvent const* event, void* context);
//    void handle_input(MirInputEvent const* event);
//    void handle_keyboard(MirKeyboardEvent const* event);
//    void handle_pointer(MirPointerEvent const* event);
//    void handle_touch(MirTouchEvent const* event);
//    void resize(int new_width, int new_height);
    void real_launch();

    void prev_app();
    void next_app();
    void run_app();

    ExternalClientLauncher& external_client_launcher;

    unsigned width = 0;
    unsigned height = 0;

    std::vector<app_details> const apps = load_details();

    std::mutex mutable mutex;
    std::vector<app_details>::const_iterator current_app{apps.begin()};
//    bool exec_currrent_app{false};
    std::atomic<bool> running{false};
//    bool stopping{false};
//    mir::optional_value<mir::geometry::Size> resize_;
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
    puts(__PRETTY_FUNCTION__);
    if (auto ss = self.lock())
    {
        puts(" . . Calling start()");
        ss->start();
    }
}

void egmde::Launcher::operator()(wl_display* display)
{
    puts(__PRETTY_FUNCTION__);
    auto client = std::make_shared<Self>(display, external_client_launcher);
    self = client;
    client->run(display);

    // Possibly need to wait for stop() to release the client.
    // (This would be less ugly with a ref-counted wrapper for wl_display* in the miral API)
    std::lock_guard<decltype(mutex)> lock{mutex};
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

        for_each_surface([this](auto& info) { draw_screen(info);});
    }
}

void egmde::Launcher::Self::launch()
{
    puts(__PRETTY_FUNCTION__);
//    {
//        std::lock_guard<decltype(mutex)> lock{mutex};
//        if (running)
//            return;
//
//        running = true;
//    }
//
//    enqueue_work(std::bind(&Self::real_launch, this));
}

void egmde::Launcher::Self::real_launch()
{
    puts(__PRETTY_FUNCTION__);
//    mc::DisplayConfig{connection}.for_each_output([this](MirOutput const* output)
//    {
//        if (!mir_output_is_enabled(output))
//            return;
//
//         width = std::max(width, mir_output_get_logical_width(output));
//         height = std::max(height, mir_output_get_logical_height(output));
//    });
//
//    surface = mc::Surface{mir_connection_create_render_surface_sync(connection, width, height)};
//    buffer_stream = mir_render_surface_get_buffer_stream(surface, width, height, mir_pixel_format_argb_8888);
//
//    window = mc::WindowSpec::for_normal_window(connection, width, height)
//            .set_name("launcher")
//            .set_event_handler(&window_event_callback, this)
//            .set_fullscreen_on_output(0)
//            .add_surface(surface, width, height, 0, 0)
//            .create_window();
//
//    stopping = false;
//
//    while (!exec_currrent_app && !stopping)
//    {
//        std::unique_lock<decltype(mutex)> lock{mutex};
//
//        if (resize_)
//        {
//            auto const& size = resize_.value();
//            mir_render_surface_set_size(surface, size.width.as_uint32_t(), size.height.as_uint32_t());
//            mir_buffer_stream_set_size(buffer_stream, size.width.as_uint32_t(), size.height.as_uint32_t());
//            mc::WindowSpec::for_changes(connection)
//                .add_surface(surface, size.width.as_uint32_t(), size.height.as_uint32_t(), 0, 0)
//                .apply_to(window);
//        }
//
//        static uint8_t const pattern[4] = { 0x1f, 0x1f, 0x1f, 0xaf };
//
//        MirGraphicsRegion region;
//        mir_buffer_stream_get_graphics_region(buffer_stream, &region);
//
//        char* row = region.vaddr;
//
//        for (int j = 0; j != region.height; ++j)
//        {
//            for (int i = 0; i < region.width; i++)
//                memcpy(row+4*i, pattern, 4);
//            row += region.stride;
//        }
//
//        // One day we'll use the icon file
//
//        static Printer printer;
//        printer.print(region, current_app->title);
//        printer.footer(region.width, region.height, reinterpret_cast<unsigned char*>(region.vaddr),
//            {"<Enter> = start app | Arrow keys = change app | initial letter = change app | <Esc> = cancel", ""});
//
//        mir_buffer_stream_swap_buffers_sync(buffer_stream);
//        height = region.height;
//        width = region.width;
//
//        if (resize_)
//        {
//            auto const& size = resize_.value();
//            if (width == size.width.as_uint32_t() && height == size.height.as_uint32_t())
//                resize_.consume();
//            else
//                continue;
//        }
//
//        cv.wait(lock);
//    }
//
//    {
//        mc::Window temp;
//        std::unique_lock<decltype(mutex)> lock{mutex};
//        temp = window;
//        window.reset();
//        buffer_stream = nullptr;
//        surface.reset();
//        running = false;
//        if (!exec_currrent_app)
//        {
//            return;
//        }
//        exec_currrent_app = false;
//    }
//
//    setenv("NO_AT_BRIDGE", "1", 1);
//    unsetenv("DISPLAY");
//
//    auto app = current_app->exec;
//    auto ws = app.find(' ');
//    if (ws != std::string::npos)
//        app.erase(ws);
//
//    static char const* launch_prefix = getenv("EGMDE_LAUNCH_PREFIX");
//
//    std::vector<std::string> command;
//
//    char const* start = nullptr;
//    char const* end = nullptr;
//
//    if (launch_prefix)
//    {
//        for (start = launch_prefix; (end = strchr(start, ' ')); start = end+1)
//        {
//            if (start != end)
//                command.emplace_back(start, end);
//        }
//
//        command.emplace_back(start);
//    }
//
//    for (start = app.c_str(); (end = strchr(start, ' ')); start = end+1)
//    {
//        if (start != end)
//            command.emplace_back(start, end);
//    }
//
//    command.emplace_back(start);
//
//    external_client_launcher.launch(command);
}

//void egmde::Launcher::Self::handle_input(MirInputEvent const* event)
//{
//    switch (mir_input_event_get_type(event))
//    {
//    case mir_input_event_type_key:
//        handle_keyboard(mir_input_event_get_keyboard_event(event));
//        break;
//
//    case mir_input_event_type_pointer:
//        handle_pointer(mir_input_event_get_pointer_event(event));
//        break;
//
//    case mir_input_event_type_touch:
//        handle_touch(mir_input_event_get_touch_event(event));
//        break;
//
//    default:;
//    }
//}
//
//void egmde::Launcher::Self::handle_keyboard(MirKeyboardEvent const* event)
//{
//    if (mir_keyboard_event_action(event) == mir_keyboard_action_down)
//        switch (mir_keyboard_event_scan_code(event))
//        {
//        case KEY_RIGHT:
//        case KEY_DOWN:
//        {
//            std::lock_guard<decltype(mutex)> lock{mutex};
//            next_app();
//            break;
//        }
//
//        case KEY_LEFT:
//        case KEY_UP:
//        {
//            std::lock_guard<decltype(mutex)> lock{mutex};
//            prev_app();
//            break;
//        }
//
//        case KEY_ENTER:
//        case KEY_SPACE:
//        {
//            std::lock_guard<decltype(mutex)> lock{mutex};
//            run_app();
//            break;
//        }
//
//        case KEY_ESC:
//        {
//            std::lock_guard<decltype(mutex)> lock{mutex};
//            stopping = true;
//            cv.notify_one();
//            break;
//        }
//
//        default:
//        {
//            auto const temp = mir_keyboard_event_key_text(event);
//
//            if (isalnum(*temp))
//            {
//                char const text[] = {static_cast<char>(toupper(*temp)), '\0'};
//
//                auto p = current_app + 1;
//                auto end = apps.end();
//
//                if (p == end || text < current_app->name.substr(0,1))
//                {
//                    p = apps.begin();
//                    end = current_app;
//                }
//
//                while (text > p->name.substr(0,1) && p != apps.end())
//                    ++p;
//
//                if (p != apps.end())
//                {
//                    current_app = p;
//                    cv.notify_one();
//                }
//            }
//        }
//        }
//}
//
//void egmde::Launcher::Self::handle_pointer(MirPointerEvent const* event)
//{
//    if (mir_pointer_event_action(event) == mir_pointer_action_button_up)
//    {
//        std::lock_guard<decltype(mutex)> lock{mutex};
//        auto const y = mir_pointer_event_axis_value(event, mir_pointer_axis_y);
//
//        if (y < height/3)
//            prev_app();
//        else if (y > (2*height)/3)
//            next_app();
//        else
//            run_app();
//    }
//}
//
//void egmde::Launcher::Self::handle_touch(MirTouchEvent const* event)
//{
//    auto const count = mir_touch_event_point_count(event);
//
//    if (count == 1 && mir_touch_event_action(event, 0) == mir_touch_action_up)
//    {
//        if (mir_touch_event_axis_value(event, 0, mir_touch_axis_x) < 5)
//        {
//            std::lock_guard<decltype(mutex)> lock{mutex};
//            stopping = true;
//            cv.notify_one();
//            return;
//        }
//
//        auto const y = mir_touch_event_axis_value(event, 0, mir_touch_axis_y);
//
//        std::lock_guard<decltype(mutex)> lock{mutex};
//        if (y < height/3)
//            prev_app();
//        else if (y > (2*height)/3)
//            next_app();
//        else
//            run_app();
//    }
//}

void egmde::Launcher::Self::run_app()
{
//    exec_currrent_app = true;
//    cv.notify_one();
}

void egmde::Launcher::Self::next_app()
{
//    if (++current_app == apps.end())
//        current_app = apps.begin();
//
//    cv.notify_one();
}

void egmde::Launcher::Self::prev_app()
{
//    if (current_app == apps.begin())
//        current_app = apps.end();
//
//    --current_app;
//
//    cv.notify_one();
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
    puts(__PRETTY_FUNCTION__);
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
    wl_display_roundtrip(display);
}

void egmde::Launcher::Self::clear_screen(SurfaceInfo& info) const
{
    info.clear_window();
}
