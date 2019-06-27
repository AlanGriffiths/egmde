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

#include "gtk_primary_selection.h"
#include "egprimary_selection_device_controller.h"

#include "wayland-generated/gtk-primary-selection_wrapper.h"

using namespace mir::wayland;
using namespace miral;

namespace
{
class PrimarySelectionDeviceManager : public GtkPrimarySelectionDeviceManager
{
public:
    PrimarySelectionDeviceManager(
        struct wl_resource* resource,
        egmde::PrimarySelectionDeviceController* controller);

private:
    egmde::PrimarySelectionDeviceController* const controller;

    void create_source(struct wl_resource* id) override;

    void get_device(struct wl_resource* id, struct wl_resource* seat) override;

    void destroy() override;
};

class PrimarySelectionOffer;

class PrimarySelectionDevice : public GtkPrimarySelectionDevice, public egmde::PrimarySelectionDeviceController::Device
{
public:
    PrimarySelectionDevice(
        struct wl_resource* resource, egmde::PrimarySelectionDeviceController* controller);

private:
    egmde::PrimarySelectionDeviceController* const controller;

    void set_selection(std::experimental::optional<struct wl_resource*> const& source, uint32_t serial) override;

    void destroy() override;

    void data_offer(egmde::PrimarySelectionDeviceController::Offer* offer) override;

    void select(egmde::PrimarySelectionDeviceController::Offer* offer) override;

    auto client() const -> wl_client* override;

    auto resource() const -> wl_resource* override;
};

class PrimarySelectionOffer : public GtkPrimarySelectionOffer, public egmde::PrimarySelectionDeviceController::Offer
{
public:
    auto resource() const -> std::experimental::optional<wl_resource*> override;

    void offer(std::string const& mime_type) override;

    void source_cancelled() override;

public:
    PrimarySelectionOffer(
        GtkPrimarySelectionDevice const& parent,
        egmde::PrimarySelectionDeviceController::Source* source,
        egmde::PrimarySelectionDeviceController* controller);

private:
    egmde::PrimarySelectionDeviceController::Source* source;
    egmde::PrimarySelectionDeviceController* const controller;

    void receive(std::string const& mime_type, mir::Fd fd) override;

    void destroy() override;
};

class PrimarySelectionSource : public GtkPrimarySelectionSource, public egmde::PrimarySelectionDeviceController::Source
{
public:
    PrimarySelectionSource(
        struct wl_resource* resource, egmde::PrimarySelectionDeviceController* controller);

private:
    egmde::PrimarySelectionDeviceController* const controller;

    void offer(std::string const& mime_type) override;

    void destroy() override;

    void cancel(egmde::PrimarySelectionDeviceController::Offer* offer) override;

    void cancelled() override;

    void create_offer_for(egmde::PrimarySelectionDeviceController::Device* device) override;

    void receive(std::string const& mime_type, mir::Fd fd) override;
};

class MyGlobal : public GtkPrimarySelectionDeviceManager::Global, public egmde::PrimarySelectionDeviceController
{
public:
    explicit MyGlobal(wl_display* display);

private:
    void bind(wl_resource* new_zwp_primary_selection_device_manager_v1) override;
};
}

void PrimarySelectionDeviceManager::create_source(struct wl_resource* id)
{
    new PrimarySelectionSource{id, controller};
}

void PrimarySelectionDeviceManager::get_device(struct wl_resource* id, struct wl_resource* /*seat*/)
{
    new PrimarySelectionDevice{id, controller};
}

void PrimarySelectionDeviceManager::destroy()
{

}

PrimarySelectionDeviceManager::PrimarySelectionDeviceManager(
    struct wl_resource* resource, egmde::PrimarySelectionDeviceController* controller) :
    GtkPrimarySelectionDeviceManager(resource, Version<1>{}),
    controller{controller}
{
}

void PrimarySelectionDevice::set_selection(std::experimental::optional<struct wl_resource*> const& source, uint32_t /*serial*/)
{
    if (source)
    {
        controller->set_selection(dynamic_cast<egmde::PrimarySelectionDeviceController::Source*>(PrimarySelectionSource::from(source.value())));
    }
    else
    {
        controller->set_selection(egmde::PrimarySelectionDeviceController::null_source);
    }
}

void PrimarySelectionDevice::destroy()
{
    controller->remove(this);
    destroy_wayland_object();
}

PrimarySelectionDevice::PrimarySelectionDevice(
    struct wl_resource* resource,
    egmde::PrimarySelectionDeviceController* controller) :
    GtkPrimarySelectionDevice(resource, Version<1>{}),
    controller{controller}
{
    controller->add(this);
}

void PrimarySelectionDevice::data_offer(egmde::PrimarySelectionDeviceController::Offer* offer)
{
    if (auto offer_resource = offer->resource())
        send_data_offer_event(offer_resource.value());
}

auto PrimarySelectionDevice::client() const -> wl_client*
{
    return GtkPrimarySelectionDevice::client;
}

auto PrimarySelectionDevice::resource() const -> wl_resource*
{
    return GtkPrimarySelectionDevice::resource;
}

void PrimarySelectionDevice::select(egmde::PrimarySelectionDeviceController::Offer* offer)
{
    send_selection_event(offer->resource());
}

PrimarySelectionOffer::PrimarySelectionOffer(
    GtkPrimarySelectionDevice const& parent,
    egmde::PrimarySelectionDeviceController::Source* source,
    egmde::PrimarySelectionDeviceController* controller) :
    GtkPrimarySelectionOffer(parent),
    source{source},
    controller{controller}
{
}

auto PrimarySelectionOffer::resource() const -> std::experimental::optional<wl_resource*>
{
    return GtkPrimarySelectionOffer::resource;
}

void PrimarySelectionOffer::offer(std::string const& mime_type)
{
    send_offer_event(mime_type);
}

void PrimarySelectionSource::offer(std::string const& mime_type)
{
    add_mime_type(mime_type);
}

void PrimarySelectionOffer::receive(std::string const& mime_type, mir::Fd fd)
{
    source->receive(mime_type, fd);
}

void PrimarySelectionOffer::destroy()
{
    source->cancel(this);
    destroy_wayland_object();
}

void PrimarySelectionOffer::source_cancelled()
{
    source = egmde::PrimarySelectionDeviceController::null_source;
}

void PrimarySelectionSource::destroy()
{
    controller->set_selection(egmde::PrimarySelectionDeviceController::null_source);
    destroy_wayland_object();
}

PrimarySelectionSource::PrimarySelectionSource(
    struct wl_resource* resource,
    egmde::PrimarySelectionDeviceController* controller) :
    GtkPrimarySelectionSource(resource, Version<1>{}),
    controller{controller}
{
}

void PrimarySelectionSource::cancelled()
{
    cancel_offers();
    send_cancelled_event();
}

void PrimarySelectionSource::create_offer_for(egmde::PrimarySelectionDeviceController::Device* device)
{
    auto parent = dynamic_cast<PrimarySelectionDevice*>(device);

    auto const offer = new PrimarySelectionOffer{*parent, this, controller};
    disclose(device, offer);
}

void PrimarySelectionSource::cancel(egmde::PrimarySelectionDeviceController::Offer* offer)
{
    cancel_offer(offer);
}

void PrimarySelectionSource::receive(std::string const& mime_type, mir::Fd fd)
{
    send_send_event(mime_type, fd);
}

MyGlobal::MyGlobal(wl_display* display) :
    GtkPrimarySelectionDeviceManager::Global(display, Version<1>{})
{
}

void MyGlobal::bind(wl_resource* new_zwp_primary_selection_device_manager_v1)
{
    new PrimarySelectionDeviceManager{new_zwp_primary_selection_device_manager_v1, this};
}

auto egmde::gtk_primary_selection_extension() -> WaylandExtensions::Builder
{
    return
        {
            PrimarySelectionDeviceManager::interface_name,
            [](WaylandExtensions::Context const* context)
            {
                return std::make_shared<MyGlobal>(context->display());
            }
        };
}
