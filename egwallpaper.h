/*
 * Copyright © 2016-2018 Octopull Ltd.
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

#ifndef EGMDE_EGWALLPAPER_H
#define EGMDE_EGWALLPAPER_H

#include "egworker.h"

#include <mir/client/connection.h>
#include <mir/client/surface.h>
#include <mir/client/window.h>

#include <miral/application.h>

#include <mutex>
#include <vector>

namespace egmde
{

class Wallpaper : Worker
{
public:
    // These operators are the protocol for an "Internal Client"
    void operator()(mir::client::Connection c) { start(std::move(c)); }
    void operator()(std::weak_ptr<mir::scene::Session> const&){ }

    // Used in initialization to set colour
    void bottom(std::string const& option);
    void top(std::string const& option);

    void start(mir::client::Connection connection);
    void stop();

private:

    uint8_t bottom_colour[4] = { 0x0a, 0x24, 0x77, 0xFF };
    uint8_t top_colour[4] = { 0x00, 0x00, 0x00, 0xFF };
    std::mutex mutable mutex;
    mir::client::Connection connection;

    struct Window
    {
        mir::client::Surface surface;
        MirBufferStream* buffer_stream = nullptr;
        mir::client::Window window;
    };

    std::vector<Window> windows;

    void create_windows();
    void handle_event(MirWindow* window, MirEvent const* ev);
    static void handle_event(MirWindow* window, MirEvent const* event, void* context);
};
}

#endif //EGMDE_EGWALLPAPER_H
