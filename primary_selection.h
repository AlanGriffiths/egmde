/*
 * Copyright © 2019 Octopull Limited
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

#ifndef EGMDE_PRIMARY_SELECTION_H
#define EGMDE_PRIMARY_SELECTION_H

#include <miral/wayland_extensions.h>

namespace egmde
{
auto primary_selection_extension() -> miral::WaylandExtensions::Builder;
}

#endif //EGMDE_PRIMARY_SELECTION_H
