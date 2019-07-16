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

#include "wayland-generated/gtk-primary-selection_wrapper.h"
#include "gtk_primary_selection.h"
#include "egprimary_selection_device_controller.h"

#include <algorithm>

namespace
{
static struct NullSource : egmde::PrimarySelectionDeviceController::Source
{
    void create_offer_for(egmde::PrimarySelectionDeviceController::Device*) override {}

    void cancelled() override {}

    void cancel(egmde::PrimarySelectionDeviceController::Offer*) override {}

    void receive(std::string const&, mir::Fd) override {}
} null_source_;
}

egmde::PrimarySelectionDeviceController::Source* const egmde::PrimarySelectionDeviceController::null_source = &null_source_;

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

void egmde::PrimarySelectionDeviceController::Source::disclose(Device* device, Offer* const offer)
{
    device->make_data_offer(offer);

    for (auto const& mime_type : mime_types)
        offer->offer(mime_type);

    device->select(offer);

    offers.push_back(offer);
}

void egmde::PrimarySelectionDeviceController::Source::add_mime_type(std::string const& mime_type)
{
    mime_types.push_back(mime_type);
}

void egmde::PrimarySelectionDeviceController::Source::cancel_offers()
{
    for (auto const offer : Source::offers)
        offer->source_cancelled();
}

void egmde::PrimarySelectionDeviceController::Source::cancel_offer(Offer* offer)
{
    Source::offers.erase(std::remove(begin(Source::offers), end(Source::offers), offer), end(Source::offers));
}

void egmde::PrimarySelectionDeviceController::Device::make_data_offer(Offer* offer)
{
    if (auto offer_resource = offer->resource())
    {
        send_data_offer(offer_resource.value());
    }
}
