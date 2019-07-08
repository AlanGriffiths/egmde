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

#ifndef EGMDE_WINDOWMANAGER_H
#define EGMDE_WINDOWMANAGER_H

#include <miral/minimal_window_manager.h>

namespace egmde
{
class Wallpaper;

class WindowManager : public miral::MinimalWindowManager
{
public:

    WindowManager(miral::WindowManagerTools const& tools, egmde::Wallpaper const& wallpaper);

    auto place_new_window(miral::ApplicationInfo const& app_info, miral::WindowSpecification const& requested_specification)
    -> miral::WindowSpecification override;

private:
    Wallpaper const* const wallpaper;
};
}


#endif //EGMDE_WINDOWMANAGER_H
