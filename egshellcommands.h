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

#ifndef EGMDE_EGSHELLCOMMANDS_H
#define EGMDE_EGSHELLCOMMANDS_H

#include <miral/application.h>
#include <miral/toolkit_event.h>

#include <atomic>
#include <functional>
#include <set>
#include <string>
#include <mutex>

using namespace miral::toolkit;
namespace miral { class MirRunner; }
namespace egmde
{
using namespace miral;

class Launcher;
class Wallpaper;
class WindowManagerPolicy;

class ShellCommands
{
public:
    ShellCommands(MirRunner& runner, Launcher& launcher, std::string const& terminal_cmd, std::function<void()> const& launch_app);

    void init_window_manager(WindowManagerPolicy* wm);

    void advise_new_window_for(Application const& app);
    void advise_delete_window_for(Application const& app);

    auto input_event(MirEvent const* event) -> bool;
    auto shell_keyboard_enabled() const -> bool
        { return shell_commands_active; }

private:
    auto keyboard_shortcuts(MirKeyboardEvent const* kev) -> bool;
    auto touch_shortcuts(MirTouchEvent const* tev) -> bool;

    MirRunner& runner;
    Launcher& launcher;
    std::string const terminal_cmd;
    std::function<void()> const& launch_app;
    WindowManagerPolicy* wm = nullptr;
    std::atomic<bool> shell_commands_active = true;

    std::mutex mutex;
    std::set<Application> shell_apps;
    int app_windows = 0;
    bool in_touch_gesture = false;
};
}

#endif //EGMDE_EGSHELLCOMMANDS_H
