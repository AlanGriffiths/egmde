/*
 * Copyright © 2016-20 Octopull Ltd.
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

#include "egwindowmanager.h"
#include "egshellcommands.h"
#include "egwallpaper.h"

#include <miral/application_info.h>
#include <miral/window_info.h>
#include <miral/window_manager_tools.h>

#include <linux/input.h>
#include <unistd.h>
#include <signal.h>

using namespace mir::geometry;


egmde::WindowManagerPolicy::WindowManagerPolicy(WindowManagerTools const& tools, Wallpaper const& wallpaper, ShellCommands&commands) :
    MinimalWindowManager{tools},
    wallpaper{&wallpaper},
    commands{&commands}
{
}

miral::WindowSpecification egmde::WindowManagerPolicy::place_new_window(
    miral::ApplicationInfo const& app_info, miral::WindowSpecification const& request_parameters)
{
    auto result = MinimalWindowManager::place_new_window(app_info, request_parameters);

    if (app_info.application() == wallpaper->session())
    {
        result.type() = mir_window_type_decoration;
    }

    return result;
}

void egmde::WindowManagerPolicy::advise_new_window(const miral::WindowInfo &window_info)
{
    WindowManagementPolicy::advise_new_window(window_info);
    if (window_info.window().application() == wallpaper->session())
    {
        commands->add_shell_app(wallpaper->session());
    }
    commands->advise_new_window_for(window_info.window().application());
}

void egmde::WindowManagerPolicy::advise_delete_window(const miral::WindowInfo &window_info)
{
    WindowManagementPolicy::advise_delete_window(window_info);
    commands->advise_delete_window_for(window_info.window().application());
}

void egmde::WindowManagerPolicy::advise_delete_app(miral::ApplicationInfo const& application)
{
    WindowManagementPolicy::advise_delete_app(application);

    commands->del_shell_app(application.application());
}

bool egmde::WindowManagerPolicy::handle_keyboard_event(MirKeyboardEvent const* kev)
{
    if (MinimalWindowManager::handle_keyboard_event(kev))
        return true;

    if (mir_keyboard_event_action(kev) != mir_keyboard_action_down)
        return false;

    auto const mods = mir_keyboard_event_modifiers(kev);

    if (!(mods & mir_input_event_modifier_alt) || !(mods & mir_input_event_modifier_ctrl))
        return false;

    if (auto active_window = tools.active_window())
    {
        auto active_output = tools.active_output();
        auto& window_info = tools.info_for(active_window);
        WindowSpecification modifications;

        switch (mir_keyboard_event_scan_code(kev))
        {
        case KEY_LEFT:
            modifications.state() = mir_window_state_vertmaximized;
            tools.place_and_size_for_state(modifications, window_info);
            modifications.top_left() = active_output.top_left;
            tools.modify_window(window_info, modifications);
            return true;

        case KEY_RIGHT:
            modifications.state() = mir_window_state_vertmaximized;
            tools.place_and_size_for_state(modifications, window_info);

            if (modifications.size().is_set())
            {
                modifications.top_left() = active_output.top_right() - as_delta(modifications.size().value().width);
            }
            else
            {
                modifications.top_left() = active_output.top_right() - as_delta(active_window.size().width);
            }

            tools.modify_window(window_info, modifications);
            return true;
        }
    }

    return false;
}
