/*
 * Copyright © 2018-2019 Octopull Limited.
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

#ifndef EGMDE_LAUNCHER_H
#define EGMDE_LAUNCHER_H

#include <miral/application.h>

#include <miral/external_client.h>

#include <memory>
#include <mutex>

struct wl_display;
namespace egmde
{
class Launcher
{
public:
    Launcher(miral::ExternalClientLauncher& external_client_launcher);

    // These operators are the protocol for an "Internal Client"
    void operator()(wl_display* display);
    void operator()(std::weak_ptr<mir::scene::Session> const&){ }

    void show();

    void stop();
    void run_app(std::string app) const;

private:
    miral::ExternalClientLauncher& external_client_launcher;
    std::mutex mutable mutex;

    struct Self;
    std::weak_ptr<Self> self;
};
}
#endif //EGMDE_LAUNCHER_H
