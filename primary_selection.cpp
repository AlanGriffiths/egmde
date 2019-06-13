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

#include "primary_selection.h"

#include "wayland-generated/primary-selection-unstable-v1_wrapper.h"

using namespace mir::wayland;
using namespace miral;

namespace
{
class PrimarySelectionDeviceManager : public PrimarySelectionDeviceManagerV1
{
public:
    using PrimarySelectionDeviceManagerV1::PrimarySelectionDeviceManagerV1;

    void create_source(struct wl_resource* id) override;

    void get_device(struct wl_resource* id, struct wl_resource* seat) override;

    void destroy() override;
};

class PrimarySelectionDevice : public PrimarySelectionDeviceV1
{
public:
    using PrimarySelectionDeviceV1::PrimarySelectionDeviceV1;

    void set_selection(std::experimental::optional<struct wl_resource*> const& source, uint32_t serial) override;

    void destroy() override;
};

class PrimarySelectionOffer : public PrimarySelectionOfferV1
{
public:
    using PrimarySelectionOfferV1::PrimarySelectionOfferV1;

    void receive(std::string const& mime_type, mir::Fd fd) override;

    void destroy() override;
};

class PrimarySelectionSource : public PrimarySelectionSourceV1
{
public:
    using PrimarySelectionSourceV1::PrimarySelectionSourceV1;

    void offer(std::string const& mime_type) override;

    void destroy() override;
};

class Global : public PrimarySelectionDeviceManagerV1::Global
{
public:
    explicit Global(wl_display* display);

    void bind(wl_resource* new_zwp_primary_selection_device_manager_v1) override;
};
}

void PrimarySelectionDeviceManager::create_source(struct wl_resource* id)
{
    new PrimarySelectionSource{id};
}

void PrimarySelectionDeviceManager::get_device(struct wl_resource* id, struct wl_resource* /*seat*/)
{
    new PrimarySelectionDevice{id};
}

void PrimarySelectionDeviceManager::destroy()
{

}


void PrimarySelectionDevice::set_selection(std::experimental::optional<struct wl_resource*> const& /*source*/, uint32_t /*serial*/)
{

}

void PrimarySelectionDevice::destroy()
{

}

void PrimarySelectionOffer::receive(std::string const& /*mime_type*/, mir::Fd /*fd*/)
{

}

void PrimarySelectionOffer::destroy()
{

}

void PrimarySelectionSource::offer(std::string const& /*mime_type*/)
{

}

void PrimarySelectionSource::destroy()
{

}

Global::Global(wl_display* display) :
    PrimarySelectionDeviceManagerV1::Global(display, 1)
{
}

void Global::bind(wl_resource* new_zwp_primary_selection_device_manager_v1)
{
    new PrimarySelectionDeviceManager{new_zwp_primary_selection_device_manager_v1};
}

auto egmde::primary_selection_extension() -> WaylandExtensions::Builder
{
    return
        {
            PrimarySelectionDeviceManager::interface_name,
            [](WaylandExtensions::Context const* context)
            {
                return std::make_shared<Global>(context->display());
            }
        };
}
