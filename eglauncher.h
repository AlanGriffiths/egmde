/*
 * Copyright © 2018 Octopull Limited.
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
#include <miral/version.h>

#if MIRAL_VERSION >= MIR_VERSION_NUMBER(2, 2, 0)
#include <miral/external_client.h>
#endif

#include <mir/client/connection.h>
#include <mir/optional_value.h>

#include <memory>

namespace egmde
{
class Launcher
{
public:
#if MIRAL_VERSION >= MIR_VERSION_NUMBER(2, 2, 0)
    Launcher(miral::ExternalClientLauncher& external_client_launcher);
#else
    Launcher();
#endif

    // These operators are the protocol for an "Internal Client"
    void operator()(mir::client::Connection c) { start(std::move(c)); }
    void operator()(std::weak_ptr<mir::scene::Session> const&){ }

    void set_login(mir::optional_value<std::string> const& user);

    void launch();

    void stop();

private:
    void start(mir::client::Connection connection);

    struct Self;
    std::shared_ptr<Self> self;
};
}
#endif //EGMDE_LAUNCHER_H
