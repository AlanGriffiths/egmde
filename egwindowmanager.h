/*
 * Copyright © 2016-19 Octopull Ltd.
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

#ifndef EGMDE_EGWINDOWMANAGER_H
#define EGMDE_EGWINDOWMANAGER_H

#include <miral/minimal_window_manager.h>

namespace egmde
{
using namespace miral;
class Wallpaper;
class ShellCommands;

class WindowManagerPolicy :
    public MinimalWindowManager
{
public:
    WindowManagerPolicy(WindowManagerTools const& tools, Wallpaper const& wallpaper, ShellCommands& commands);

    auto place_new_window(ApplicationInfo const& app_info, WindowSpecification const& request_parameters)
        -> WindowSpecification override;

    void advise_new_window(const WindowInfo &window_info) override;

    void advise_delete_app(ApplicationInfo const& application) override;

    void advise_delete_window(const WindowInfo &window_info) override;

    bool handle_keyboard_event(MirKeyboardEvent const* event) override;

private:
    Wallpaper const* const wallpaper;
    ShellCommands* const commands;
};
}

#endif //EGMDE_EGWINDOWMANAGER_H
