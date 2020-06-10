/*
 * Copyright © 2020 Octopull Ltd.
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

#include "egshellcommands.h"
#include "eglauncher.h"
#include "egwindowmanager.h"

#include <miral/runner.h>

#include <linux/input.h>

egmde::ShellCommands::ShellCommands(MirRunner& runner, Launcher& launcher, std::string const& terminal_cmd) :
    runner{runner}, launcher{launcher}, terminal_cmd{terminal_cmd}
{
}

void egmde::ShellCommands::advise_new_window_for(miral::Application const& app)
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    if (shell_apps.find(app) == shell_apps.end())
    {
        ++app_windows;
    }
}

void egmde::ShellCommands::advise_delete_window_for(miral::Application const& app)
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    if (shell_apps.find(app) == shell_apps.end())
    {
        --app_windows;
    }
}

void egmde::ShellCommands::add_shell_app(miral::Application const& app)
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    shell_apps.insert(app);
}

void egmde::ShellCommands::del_shell_app(miral::Application const& app)
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    shell_apps.erase(app);
}

auto egmde::ShellCommands::keyboard_shortcuts(MirKeyboardEvent const* kev) -> bool
{
    if (mir_keyboard_event_action(kev) == mir_keyboard_action_up)
        return false;

    MirInputEventModifiers mods = mir_keyboard_event_modifiers(kev);
    if (!(mods & mir_input_event_modifier_alt) || !(mods & mir_input_event_modifier_ctrl))
        return false;

    auto const scan_code = mir_keyboard_event_scan_code(kev);

    if (scan_code == KEY_DELETE && mir_keyboard_event_action(kev) == mir_keyboard_action_down)
    {
        shell_commands_active = !shell_commands_active;
        return true;
    }

    if (!shell_commands_active)
        return false;

    switch (scan_code)
    {
    case KEY_A:
        if (mir_keyboard_event_action(kev) != mir_keyboard_action_down)
            return false;

        add_shell_app(launcher.session());
        launcher.show();
        return true;

    case KEY_BACKSPACE:
        if (mir_keyboard_event_action(kev) == mir_keyboard_action_down)
        {
            std::lock_guard<decltype(mutex)> lock{mutex};
            if (app_windows > 0)
            {
                return false;
            }
        }
        runner.stop();
        return true;

    case KEY_T:
        if (mir_keyboard_event_action(kev) != mir_keyboard_action_down)
            return false;
        launcher.run_app(terminal_cmd, egmde::Launcher::Mode::wayland);
        return true;

    case KEY_X:
        if (mir_keyboard_event_action(kev) != mir_keyboard_action_down)
            return false;
        launcher.run_app(terminal_cmd, egmde::Launcher::Mode::x11);
        return true;

    case KEY_LEFT:
        wm->dock_active_window_left();
        return true;

    case KEY_RIGHT:
        wm->dock_active_window_right();
        return true;

    case KEY_UP:
        wm->workspace_up(mods & mir_input_event_modifier_shift);
        return true;

    case KEY_DOWN:
        wm->workspace_down(mods & mir_input_event_modifier_shift);
        return true;

    default:
        return false;
    }
}

auto egmde::ShellCommands::touch_shortcuts(MirTouchEvent const* tev) -> bool
{
    if (in_touch_gesture)
    {
        if (mir_touch_event_action(tev, 0) == mir_touch_action_up)
            in_touch_gesture = false;
        return true;
    }

    if (mir_touch_event_point_count(tev) != 1)
        return false;

    if (mir_touch_event_action(tev, 0) != mir_touch_action_down)
        return false;

    if (mir_touch_event_axis_value(tev, 0, mir_touch_axis_x) >= 5)
        return false;

    launcher.show();
    in_touch_gesture = true;
    return true;
}

auto egmde::ShellCommands::input_event(MirEvent const* event) -> bool
{
    if (mir_event_get_type(event) != mir_event_type_input)
        return false;

    auto const* input_event = mir_event_get_input_event(event);

    switch (mir_input_event_get_type(input_event))
    {
    case mir_input_event_type_touch:
        return touch_shortcuts(mir_input_event_get_touch_event(input_event));

    case mir_input_event_type_key:
        return keyboard_shortcuts(mir_input_event_get_keyboard_event(input_event));

    default:
        return false;
    }
}

void egmde::ShellCommands::init_window_manager(WindowManagerPolicy* wm)
{
    this->wm = wm;
}
