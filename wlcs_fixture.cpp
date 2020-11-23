/*
 * Copyright © 2018-2019 Canonical Ltd.
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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 *              Alan Griffiths <alan@octopull.co.uk>
 */

#include "primary_selection.h"
#include "gtk_primary_selection.h"

#include <miral/wayland_extensions.h>
#include <miral/test_wlcs_display_server.h>

namespace
{
auto const primary_selection_extension = egmde::primary_selection_extension();
auto const gtk_primary_selection_extension = egmde::gtk_primary_selection_extension();

struct TestWlcsDisplayServer : miral::TestWlcsDisplayServer
{
    miral::WaylandExtensions wayland_extensions;

    TestWlcsDisplayServer(int argc, char const** argv) :
        miral::TestWlcsDisplayServer{argc, argv}
    {
        wayland_extensions.add_extension(primary_selection_extension);
        wayland_extensions.add_extension(gtk_primary_selection_extension);
        add_server_init(wayland_extensions);
    }
};

WlcsExtensionDescriptor const extensions[] = {
    {primary_selection_extension.name.c_str(), 1},
    {gtk_primary_selection_extension.name.c_str(), 1},
};

WlcsIntegrationDescriptor const descriptor{
    1,
    sizeof(extensions) / sizeof(extensions[0]),
    extensions
};

WlcsIntegrationDescriptor const* get_descriptor(WlcsDisplayServer const* /*server*/)
{
    return &descriptor;
}

WlcsDisplayServer* wlcs_create_server(int argc, char const** argv)
{
    auto server = new TestWlcsDisplayServer(argc, argv);

    server->get_descriptor = &get_descriptor;
    return server;
}

void wlcs_destroy_server(WlcsDisplayServer* server)
{
    delete static_cast<TestWlcsDisplayServer*>(server);
}
}

extern WlcsServerIntegration const wlcs_server_integration {
    1,
    &wlcs_create_server,
    &wlcs_destroy_server,
};
