/*
 * Copyright © 2016-2019 Octopull Ltd.
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

#include "egwallpaper.h"
#include "egwindowmanager.h"
#include "eglauncher.h"

#include <miral/append_event_filter.h>
#include <miral/command_line_option.h>
#include <miral/display_configuration_option.h>
#include <miral/internal_client.h>
#include <miral/keymap.h>
#include <miral/runner.h>
#include <miral/set_window_management_policy.h>
#include <miral/wayland_extensions.h>
#include <miral/x11_support.h>

#include <boost/filesystem.hpp>
#include <linux/input.h>

using namespace miral;

namespace
{
// Neither xdg-terminal nor x-terminal-emulator is guaranteed to exist,
// and neither is a good way to identify user preference...
std::string const terminal_cmd = []() -> std::string
    {
        auto const user_bin = "/usr/bin/";

        for (std::string name : { "weston-terminal", "gnome-terminal", "konsole",
                                  "qterminal", "lxterminal", "xdg-terminal"})
        {
            if (boost::filesystem::exists(user_bin + name))
                return name;
        }

        return "x-terminal-emulator";
    }();
}

int main(int argc, char const* argv[])
{
    MirRunner runner{argc, argv};

    egmde::Wallpaper wallpaper;

    ExternalClientLauncher external_client_launcher;
    egmde::Launcher launcher{external_client_launcher};

    auto const keyboard_shortcuts = [&](MirEvent const* event)
        {
            if (mir_event_get_type(event) != mir_event_type_input)
                return false;

            MirInputEvent const* input_event = mir_event_get_input_event(event);
            if (mir_input_event_get_type(input_event) != mir_input_event_type_key)
                return false;

            MirKeyboardEvent const* kev = mir_input_event_get_keyboard_event(input_event);
            if (mir_keyboard_event_action(kev) != mir_keyboard_action_down)
                return false;

            MirInputEventModifiers mods = mir_keyboard_event_modifiers(kev);
            if (!(mods & mir_input_event_modifier_alt) || !(mods & mir_input_event_modifier_ctrl))
                return false;

            switch (mir_keyboard_event_scan_code(kev))
            {
            case KEY_A:launcher.show();
                return true;

            case KEY_BACKSPACE:
                runner.stop();
                return true;

            case KEY_T: launcher.run_app(terminal_cmd, egmde::Launcher::Mode::wayland);
                return true;

            case KEY_X: launcher.run_app(terminal_cmd, egmde::Launcher::Mode::x11);
                return true;

            default:
                return false;
            }
        };

    auto touch_shortcuts = [&, gesture = false](MirEvent const* event) mutable
        {
            if (mir_event_get_type(event) != mir_event_type_input)
                return false;

            auto const* input_event = mir_event_get_input_event(event);
            if (mir_input_event_get_type(input_event) != mir_input_event_type_touch)
                return false;

            auto const* tev = mir_input_event_get_touch_event(input_event);

            if (gesture)
            {
                if (mir_touch_event_action(tev, 0) == mir_touch_action_up)
                    gesture = false;
                return true;
            }

            if (mir_touch_event_point_count(tev) != 1)
                return false;

            if (mir_touch_event_action(tev, 0) != mir_touch_action_down)
                return false;

            if (mir_touch_event_axis_value(tev, 0, mir_touch_axis_x) >= 5)
                return false;

            launcher.show();
            gesture = true;
            return true;
        };


    runner.add_stop_callback([&] { wallpaper.stop(); });
    runner.add_stop_callback([&] { launcher.stop(); });

    return runner.run_with(
        {
            X11Support{},
            WaylandExtensions{},
            display_configuration_options,
            CommandLineOption{[&](auto& option) { wallpaper.top(option);},
                              "wallpaper-top",    "Colour of wallpaper RGB", "0x000000"},
            CommandLineOption{[&](auto& option) { wallpaper.bottom(option);},
                              "wallpaper-bottom", "Colour of wallpaper RGB", EGMDE_WALLPAPER_BOTTOM},
            StartupInternalClient{std::ref(wallpaper)},
            external_client_launcher,
            StartupInternalClient{std::ref(launcher)},
            Keymap{},
            AppendEventFilter{keyboard_shortcuts},
            AppendEventFilter{touch_shortcuts},
            set_window_management_policy<egmde::WindowManagerPolicy>(wallpaper)
        });
}
