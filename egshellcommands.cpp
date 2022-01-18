/*
 * Copyright © 2022 Octopull Ltd.
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

#include <xkbcommon/xkbcommon-keysyms.h>

#include <utility>

egmde::ShellCommands::ShellCommands(MirRunner& runner, Launcher& launcher, std::string  terminal_cmd, std::function<void()> const& launch_app) :
    runner{runner}, launcher{launcher}, terminal_cmd{std::move(terminal_cmd)}, launch_app{launch_app}
{
}

void egmde::ShellCommands::advise_new_window_for(miral::Application const& /*app*/)
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    ++app_windows;
}

void egmde::ShellCommands::advise_delete_window_for(miral::Application const& /*app*/)
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    --app_windows;
}

auto egmde::ShellCommands::keyboard_shortcuts(MirKeyboardEvent const* kev) -> bool
{
    if (mir_keyboard_event_action(kev) == mir_keyboard_action_up)
        return false;

    MirInputEventModifiers mods = mir_keyboard_event_modifiers(kev);
    if (!(mods & mir_input_event_modifier_alt) || !(mods & mir_input_event_modifier_ctrl))
        return false;

    auto const key_code = mir_keyboard_event_key_code(kev);

    if (key_code == XKB_KEY_Delete && mir_keyboard_event_action(kev) == mir_keyboard_action_down)
    {
        shell_commands_active = !shell_commands_active;
        return true;
    }

    if (!shell_commands_active)
        return false;

    switch (key_code)
    {
    case XKB_KEY_A:
    case XKB_KEY_a:
        if (mir_keyboard_event_action(kev) != mir_keyboard_action_down)
            return false;

        launch_app();
        return true;

    case XKB_KEY_BackSpace:
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

    case XKB_KEY_T:
    case XKB_KEY_t:
        if (mir_keyboard_event_action(kev) != mir_keyboard_action_down)
            return false;
        launcher.run_app(terminal_cmd, egmde::Launcher::Mode::wayland);
        return true;

    case XKB_KEY_X:
    case XKB_KEY_x:
        if (mir_keyboard_event_action(kev) != mir_keyboard_action_down)
            return false;
        launcher.run_app(terminal_cmd, egmde::Launcher::Mode::x11);
        return true;

    case XKB_KEY_Left:
        wm->dock_active_window_left();
        return true;

    case XKB_KEY_Right:
        wm->dock_active_window_right();
        return true;

    case XKB_KEY_space:
        wm->toggle_maximized_restored();
        return true;

    case XKB_KEY_Up:
        wm->workspace_up(mods & mir_input_event_modifier_shift);
        return true;

    case XKB_KEY_Down:
        wm->workspace_down(mods & mir_input_event_modifier_shift);
        return true;

    case XKB_KEY_bracketright:
        wm->focus_next_application();
        return true;

    case XKB_KEY_bracketleft:
        wm->focus_prev_application();
        return true;

    case XKB_KEY_braceright:
        wm->focus_next_within_application();
        return true;

    case XKB_KEY_braceleft:
        wm->focus_prev_within_application();
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

    launch_app();
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
