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

#include <miral/runner.h>
#include <miral/append_event_filter.h>
#include <miral/external_client.h>
#include <miral/keymap.h>
#include <miral/minimal_window_manager.h>
#include <miral/set_window_management_policy.h>

#include <linux/input.h>

using namespace miral;

int main(int argc, char const* argv[])
{
    MirRunner runner{argc, argv};

    ExternalClientLauncher external_client_launcher;

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
            case KEY_BACKSPACE:
                runner.stop();
                return true;

            case KEY_T:
                external_client_launcher.launch({"weston-terminal"});
                return true;

            default:
                return false;
            }
        };

    return runner.run_with(
        {
            set_window_management_policy<MinimalWindowManager>(),
            external_client_launcher,
            AppendEventFilter{keyboard_shortcuts},
            Keymap{},
        });
}
