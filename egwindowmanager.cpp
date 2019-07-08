/*
 * Copyright © 2019 Octopull Ltd.
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

#include "egwallpaper.h"

#include <miral/application_info.h>

egmde::WindowManager::WindowManager(miral::WindowManagerTools const& tools, egmde::Wallpaper const& wallpaper) :
    MinimalWindowManager{tools},
    wallpaper{&wallpaper}
{
}

auto egmde::WindowManager::place_new_window(
    miral::ApplicationInfo const& app_info,
    miral::WindowSpecification const& requested_specification) -> miral::WindowSpecification
{
    auto result = MinimalWindowManager::place_new_window(app_info, requested_specification);

    if (app_info.application() == wallpaper->session())
    {
        // There's currently no Wayland extension that allows a client to exploit the
        // rich semantics of Mir's window type systems. But, as this is an internal,
        // client we can work-around it by setting the type ourselves.
        result.type() = mir_window_type_decoration;
    }

    return result;
}
