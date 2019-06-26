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

#include "egprimary_selection_device_controller.h"

#include <algorithm>

egmde::PrimarySelectionDeviceController::NullOffer egmde::PrimarySelectionDeviceController::null_offer;
egmde::PrimarySelectionDeviceController::NullSource egmde::PrimarySelectionDeviceController::null_source;

void egmde::PrimarySelectionDeviceController::set_selection(PrimarySelectionDeviceController::Source* source)
{
    current_selection->cancelled();
    current_selection = source;

    for (auto const device : devices)
    {
        current_selection->create_offer_for(device);
    }
}

void egmde::PrimarySelectionDeviceController::add(PrimarySelectionDeviceController::Device* device)
{
    current_selection->create_offer_for(device);
    devices.push_back(device);
}

void egmde::PrimarySelectionDeviceController::remove(PrimarySelectionDeviceController::Device* device)
{
    devices.erase(std::remove(begin(devices), end(devices), device), end(devices));
}
