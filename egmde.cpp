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

#include "egwindowmanager.h"

#include <miral/runner.h>
#include <miral/set_window_management_policy.h>

using namespace miral;

int main(int argc, char const* argv[])
{
    MirRunner runner{argc, argv};

    return runner.run_with(
        {
            set_window_management_policy<egmde::WindowManagerPolicy>()
        });
}
