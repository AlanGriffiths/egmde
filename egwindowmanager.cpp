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
