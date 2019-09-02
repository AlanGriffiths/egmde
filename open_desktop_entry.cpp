/*
 * Copyright © 2019 Canonical Ltd.
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

#include "open_desktop_entry.h"

#include <gio/gio.h>
#include <memory>

namespace
{
class Connection : std::shared_ptr<GDBusConnection>
{
public:
    explicit Connection(GDBusConnection* connection) : std::shared_ptr<GDBusConnection>{connection, &g_object_unref} {}

    operator GDBusConnection*() const { return get(); }

private:
    friend void g_object_unref(GDBusConnection*) = delete;
};
}

void egmde::open_desktop_entry(std::string const& desktop_file)
{
    Connection const connection{g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr)};

    static char const* const dest = "io.snapcraft.Launcher";
    static char const* const object_path = "/io/snapcraft/Launcher";
    static char const* const interface_name = "io.snapcraft.Launcher";
    static char const* const method_name = "OpenDesktopEntry";

    // For some reason, if this autostarts userd, then the call fails
    // but as userd has now been started, a second attempt succeeds
    for (int tries = 0; tries != 2; ++tries)
    {
        GError* error = nullptr;

        if (auto const result = g_dbus_connection_call_sync(connection,
                                                            dest,
                                                            object_path,
                                                            interface_name,
                                                            method_name,
                                                            g_variant_new("(s)", desktop_file.c_str()),
                                                            nullptr,
                                                            G_DBUS_CALL_FLAGS_NONE,
                                                            G_MAXINT,
                                                            nullptr,
                                                            &error))
        {
            g_variant_unref(result);
            break;
        }

        if (error)
        {
            puts(error->message);
            g_error_free(error);
        }
    }
}
