/*
 * Copyright © 2016 Octopull Limited.
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
#include "printer.h"
#include "egworker.h"

#include <mir/client/connection.h>
#include <mir/client/display_config.h>
#include <mir/client/surface.h>
#include <mir/client/window.h>
#include <mir/client/window_spec.h>

#include <mir_toolkit/mir_buffer_stream.h>

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
#include <locale>
#include <mutex>
#include <string>
#include <vector>
#include <stdlib.h>
#include <thread>

#include <vector>

namespace mc = mir::client;

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
        {
            for (boost::filesystem::directory_iterator i(path), end; i != end; ++i)
            {
                if (!is_directory(*i) && ends_with_desktop(i->path().filename().string()))
                {
                    list.push_back(i->path());
                }
            }
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
        auto ws = app.find(' ');
        if (ws != std::string::npos)
            app.erase(ws);

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
    std::string search_path{"~/.local/share/applications:/usr/share/applications"};
    // search_paths relies on a ":" sentinal value
    search_path +=  ":";
    auto const paths = search_paths(search_path.c_str());
    return scan_for_desktop_files(paths);

}
}

class egmde::Launcher::Self : public Worker
{
public:
    void start(mir::client::Connection connection);

    void launch();
    void stop();
    void set_login(mir::optional_value<std::string> const& user);

private:
    static void lifecycle_event_callback(MirConnection* /*connection*/, MirLifecycleState state, void* context);
    static void window_event_callback(MirWindow* window, MirEvent const* event, void* context);
    void handle_input(MirInputEvent const* event);
    void handle_keyboard(MirKeyboardEvent const* event);
    void handle_pointer(MirPointerEvent const* event);
    void handle_touch(MirTouchEvent const* event);
    void resize(int new_width, int new_height);
    void real_launch();
    void real_resize(int new_width, int new_height);
    void prev_app();
    void next_app();
    void run_app();

    mc::Connection connection;
    mc::Surface surface;
    MirBufferStream* buffer_stream{nullptr};
    mc::Window window;
    unsigned width = 0;
    unsigned height = 0;
    mir::optional_value<std::string> login;

    std::vector<app_details> const apps = load_details();

    std::mutex mutable mutex;
    std::condition_variable mutable cv;
    std::vector<app_details>::const_iterator current_app{apps.begin()};
    bool exec_currrent_app{false};
    bool stopping{false};
};

egmde::Launcher::Launcher() : self{std::make_shared<Self>()}
{
}

void egmde::Launcher::start(mc::Connection connection)
{
    self->start(std::move(connection));
}

void egmde::Launcher::stop()
{
    self->stop();
}

void egmde::Launcher::launch()
{
    self->launch();
}

void egmde::Launcher::set_login(mir::optional_value<std::string> const& user)
{
    self->set_login(user);
}

void egmde::Launcher::Self::start(mir::client::Connection connection_)
{
    connection = std::move(connection_);
    mir_connection_set_lifecycle_event_callback(connection, &lifecycle_event_callback, this);

    auto const new_terminal = std::find_if(begin(apps), end(apps),
        [](app_details const& app) { return app.name == "Terminal"; });

    if (new_terminal != end(apps))
        current_app = new_terminal;

    start_work();
}

void egmde::Launcher::Self::launch()
{
    enqueue_work(std::bind(&Self::real_launch, this));
}

void egmde::Launcher::Self::real_launch()
{
    mc::DisplayConfig{connection}.for_each_output([this](MirOutput const* output)
    {
        if (!mir_output_is_enabled(output))
            return;

         width = std::max(width, mir_output_get_logical_width(output));
         height = std::max(height, mir_output_get_logical_height(output));
    });

    surface = mc::Surface{mir_connection_create_render_surface_sync(connection, width, height)};
    buffer_stream = mir_render_surface_get_buffer_stream(surface, width, height, mir_pixel_format_argb_8888);

    window = mc::WindowSpec::for_normal_window(connection, width, height)
            .set_name("launcher")
            .set_event_handler(&window_event_callback, this)
            .set_fullscreen_on_output(0)
            .add_surface(surface, width, height, 0, 0)
            .create_window();

    while (!exec_currrent_app && !stopping)
    {
        std::unique_lock<decltype(mutex)> lock{mutex};

        static uint8_t const pattern[4] = { 0x1f, 0x1f, 0x1f, 0xaf };

        MirGraphicsRegion region;
        mir_buffer_stream_get_graphics_region(buffer_stream, &region);

        char* row = region.vaddr;

        for (int j = 0; j != region.height; ++j)
        {
            for (int i = 0; i < region.width; i++)
                memcpy(row+4*i, pattern, 4);
            row += region.stride;
        }

        // One day we'll use the icon file

        static Printer printer;
        printer.print(region, current_app->title);

        mir_buffer_stream_swap_buffers_sync(buffer_stream);
        height = region.height;

        cv.wait(lock);
    }

    {
        mc::Window temp;
        std::unique_lock<decltype(mutex)> lock{mutex};
        temp = window;
        window.reset();
        buffer_stream = nullptr;
        surface.reset();
        stopping = false;

        if (!exec_currrent_app)
        {
            return;
        }
        exec_currrent_app = false;
    }


    if (!fork())
    {
        // TODO don't hard code MIR_SOCKET & WAYLAND_DISPLAY value
        setenv("MIR_SOCKET", "/run/user/1000/egmde_socket", 1);
        setenv("WAYLAND_DISPLAY", "egmde_wayland", 1);
        setenv("GDK_BACKEND", "wayland", 1);
        setenv("QT_QPA_PLATFORM", "wayland", 1);
        setenv("SDL_VIDEODRIVER", "wayland", 1);
        setenv("NO_AT_BRIDGE", "1", 1);

        auto app = current_app->exec;
        auto ws = app.find(' ');
        if (ws != std::string::npos)
            app.erase(ws);

        if (login.is_set())
        {
            if (app == "gnome-terminal")
                app +=  " --app-id uk.co.octopull.egmde.Terminal";

            char const* exec_args[] = {
                "su",
                "--preserve-environment",
                "--command",
                nullptr,
                login.value().c_str(),
                nullptr };

            exec_args[3] = app.c_str();

            execvp(exec_args[0], const_cast<char*const*>(exec_args));
        }
        else
        {
            char const* exec_args[] = { "gnome-terminal", "--app-id", "uk.co.octopull.egmde.Terminal", nullptr };

            if (app != exec_args[0])
            {
                exec_args[0] = app.c_str();
                exec_args[1] = nullptr;
            }

            execvp(exec_args[0], const_cast<char*const*>(exec_args));
        }
    }
}


void egmde::Launcher::Self::lifecycle_event_callback(MirConnection* /*connection*/, MirLifecycleState state, void* context)
{
    switch (state)
    {
    case mir_lifecycle_state_will_suspend:
    case mir_lifecycle_state_resumed:
        return;

    case mir_lifecycle_connection_lost:
        auto self = (Self*)context;
        self->stop_work();
        break;
    }
}

void egmde::Launcher::Self::window_event_callback(MirWindow* /*window*/, MirEvent const* event, void* context)
{
    switch (mir_event_get_type(event))
    {
    case mir_event_type_input:
    {
        auto const input_event = mir_event_get_input_event(event);
        auto self = (Self*)context;
        self->handle_input(input_event);
        break;
    }

    case mir_event_type_resize:
    {
        auto const self = (Self*)context;
        auto const resize = mir_event_get_resize_event(event);
        auto const new_width = mir_resize_event_get_width(resize);
        auto const new_height= mir_resize_event_get_height(resize);

        self->resize(new_width, new_height);
        break;
    }

    case mir_event_type_window:
    {
        auto window_event = mir_event_get_window_event(event);
        if (mir_window_attrib_focus == mir_window_event_get_attribute(window_event) &&
            mir_window_focus_state_unfocused == mir_window_event_get_attribute_value(window_event))
        {
            auto const self = (Self*)context;
            std::lock_guard<decltype(self->mutex)> lock{self->mutex};
            self->stopping = true;
            self->cv.notify_one();
        }
        break;
    }

    case mir_event_type_close_window:
    {
        auto self = (Self*)context;
        self->stop_work();
        break;
    }
    default:
        ;
    }
}

void egmde::Launcher::Self::resize(int new_width, int new_height)
{
    enqueue_work(std::bind(&Self::real_resize, this, new_width, new_height));
}

void egmde::Launcher::Self::real_resize(int new_width, int new_height)
{
    mir_render_surface_set_size(surface, new_width, new_height);
    mir_buffer_stream_set_size(buffer_stream, new_width, new_height);
    mc::WindowSpec::for_changes(connection)
        .add_surface(surface, new_width, new_height, 0, 0)
        .apply_to(window);
}

void egmde::Launcher::Self::handle_input(MirInputEvent const* event)
{
    switch (mir_input_event_get_type(event))
    {
    case mir_input_event_type_key:
        handle_keyboard(mir_input_event_get_keyboard_event(event));
        break;

    case mir_input_event_type_pointer:
        handle_pointer(mir_input_event_get_pointer_event(event));
        break;

    case mir_input_event_type_touch:
        handle_touch(mir_input_event_get_touch_event(event));
        break;

    default:;
    }
}

void egmde::Launcher::Self::handle_keyboard(MirKeyboardEvent const* event)
{
    if (mir_keyboard_event_action(event) == mir_keyboard_action_down)
        switch (mir_keyboard_event_scan_code(event))
        {
        case KEY_RIGHT:
        case KEY_DOWN:
        {
            std::lock_guard<decltype(mutex)> lock{mutex};
            next_app();
            break;
        }

        case KEY_LEFT:
        case KEY_UP:
        {
            std::lock_guard<decltype(mutex)> lock{mutex};
            prev_app();
            break;
        }

        case KEY_ENTER:
        case KEY_SPACE:
        {
            std::lock_guard<decltype(mutex)> lock{mutex};
            run_app();
            break;
        }

        case KEY_ESC:
        {
            std::lock_guard<decltype(mutex)> lock{mutex};
            stopping = true;
            cv.notify_one();
            break;
        }

        default:
        {
            auto const temp = mir_keyboard_event_key_text(event);

            if (isalnum(*temp))
            {
                char const text[] = {static_cast<char>(toupper(*temp)), '\0'};

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
                    cv.notify_one();
                }
            }
        }
        }
}

void egmde::Launcher::Self::handle_pointer(MirPointerEvent const* event)
{
    if (mir_pointer_event_action(event) == mir_pointer_action_button_up)
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
        auto const y = mir_pointer_event_axis_value(event, mir_pointer_axis_y);

        if (y < height/3)
            prev_app();
        else if (y > (2*height)/3)
            next_app();
        else
            run_app();
    }
}

void egmde::Launcher::Self::handle_touch(MirTouchEvent const* event)
{
    auto const count = mir_touch_event_point_count(event);

    if (count == 1 && mir_touch_event_action(event, 0) == mir_touch_action_up)
    {
        auto const y = mir_touch_event_axis_value(event, 0, mir_touch_axis_y);

        std::lock_guard<decltype(mutex)> lock{mutex};
        if (y < height/3)
            prev_app();
        else if (y > (2*height)/3)
            next_app();
        else
            run_app();
    }
}

void egmde::Launcher::Self::run_app()
{
    exec_currrent_app = true;
    cv.notify_one();
}

void egmde::Launcher::Self::next_app()
{
    if (++current_app == apps.end())
        current_app = apps.begin();

    cv.notify_one();
}

void egmde::Launcher::Self::prev_app()
{
    if (current_app == apps.begin())
        current_app = apps.end();

    --current_app;

    cv.notify_one();
}

void egmde::Launcher::Self::stop()
{
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        stopping = true;
        cv.notify_one();
    }
    stop_work();
}

void egmde::Launcher::Self::set_login(mir::optional_value<std::string> const& user)
{
    this->login = user;
}
