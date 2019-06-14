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

#include "primary_selection.h"

#include "wayland-generated/primary-selection-unstable-v1_wrapper.h"

#include <algorithm>

using namespace mir::wayland;
using namespace miral;

// Workaround https://github.com/MirServer/mir/issues/871
namespace mir { namespace wayland {
extern struct wl_interface const zwp_primary_selection_offer_v1_interface_data;
}}

namespace
{
class PrimarySelectionDeviceController
{
public:
    PrimarySelectionDeviceController() = default;
    virtual ~PrimarySelectionDeviceController() = default;
    PrimarySelectionDeviceController(PrimarySelectionDeviceController const&) = delete;
    PrimarySelectionDeviceController& operator=(PrimarySelectionDeviceController const&) = delete;

    class Offer
    {
    public:
        Offer() = default;
        virtual ~Offer() = default;
        Offer(Offer const&) = delete;
        Offer& operator=(Offer const&) = delete;

        virtual auto resource() const -> std::experimental::optional<wl_resource*> = 0;
        virtual void offer(std::string const& mime_type) = 0;
        virtual void source_cancelled() = 0;
    };

    class Device
    {
    public:
        Device() = default;
        virtual ~Device() = default;
        Device(Device const&) = delete;
        Device& operator=(Device const&) = delete;

        virtual void data_offer(Offer* offer) = 0;
        virtual void select(Offer* offer) = 0;

        virtual auto client() const -> wl_client* = 0;
        virtual auto resource() const -> wl_resource* = 0;
    };

    class Source
    {
    public:
        Source() = default;
        virtual ~Source() = default;
        Source(Source const&) = delete;
        Source& operator=(Source const&) = delete;

        virtual void cancelled() = 0;
        virtual void create_offer_for(Device*) = 0;
        virtual void cancel(Offer* offer) = 0;
        virtual void receive(std::string const& mime_type, mir::Fd fd) = 0;
    };

    void set_selection(Source* source);
    void add(Device* device);
    void remove(Device* device);

    static struct NullOffer : Offer
    {
        auto resource() const -> std::experimental::optional<wl_resource*> override { return {}; }
        void offer(std::string const&) override {}
        void source_cancelled() override {}
    } null_offer;

    static struct NullSource : Source
    {
        void create_offer_for(Device*) override {}
        void cancelled() override {}
        void cancel(Offer*) override {}
        void receive(std::string const&, mir::Fd) override {}
    } null_source;

private:
    Source* current_selection = &null_source;

    std::vector<Device*> devices;
};

PrimarySelectionDeviceController::NullOffer PrimarySelectionDeviceController::null_offer;
PrimarySelectionDeviceController::NullSource PrimarySelectionDeviceController::null_source;

void PrimarySelectionDeviceController::set_selection(PrimarySelectionDeviceController::Source* source)
{
    current_selection->cancelled();
    current_selection = source;

    for (auto const device : devices)
    {
        current_selection->create_offer_for(device);
    }
}

void PrimarySelectionDeviceController::add(PrimarySelectionDeviceController::Device* device)
{
    current_selection->create_offer_for(device);
    devices.push_back(device);
}

void PrimarySelectionDeviceController::remove(PrimarySelectionDeviceController::Device* device)
{
    devices.erase(std::remove(begin(devices), end(devices), device), end(devices));
}

class PrimarySelectionDeviceManager : public PrimarySelectionDeviceManagerV1
{
public:
    PrimarySelectionDeviceManager(
        struct wl_resource* resource,
        PrimarySelectionDeviceController* controller);

private:
    PrimarySelectionDeviceController* const controller;

    void create_source(struct wl_resource* id) override;

    void get_device(struct wl_resource* id, struct wl_resource* seat) override;

    void destroy() override;
};

class PrimarySelectionOffer;

class PrimarySelectionDevice : public PrimarySelectionDeviceV1, PrimarySelectionDeviceController::Device
{
public:
    PrimarySelectionDevice(
        struct wl_resource* resource, PrimarySelectionDeviceController* controller);

private:
    PrimarySelectionDeviceController* const controller;

    void set_selection(std::experimental::optional<struct wl_resource*> const& source, uint32_t serial) override;

    void destroy() override;

    void data_offer(PrimarySelectionDeviceController::Offer* offer) override;

    void select(PrimarySelectionDeviceController::Offer* offer) override;

    auto client() const -> wl_client* override;

    auto resource() const -> wl_resource* override;
};

class PrimarySelectionOffer : public PrimarySelectionOfferV1, public PrimarySelectionDeviceController::Offer
{
public:
    auto resource() const -> std::experimental::optional<wl_resource*> override;

    void offer(std::string const& mime_type) override;

    void source_cancelled() override;

public:
    PrimarySelectionOffer(
        struct wl_resource* resource,
        PrimarySelectionDeviceController::Source* source,
        PrimarySelectionDeviceController* controller);

private:
    PrimarySelectionDeviceController::Source* source;
    PrimarySelectionDeviceController* const controller;

    void receive(std::string const& mime_type, mir::Fd fd) override;

    void destroy() override;
};

class PrimarySelectionSource : public PrimarySelectionSourceV1, PrimarySelectionDeviceController::Source
{
public:
    PrimarySelectionSource(
        struct wl_resource* resource, PrimarySelectionDeviceController* controller);

private:
    PrimarySelectionDeviceController* const controller;

    void offer(std::string const& mime_type) override;

    void destroy() override;

    void cancel(PrimarySelectionDeviceController::Offer* offer) override;

    void cancelled() override;

    void create_offer_for(PrimarySelectionDeviceController::Device* device) override;

    void receive(std::string const& mime_type, mir::Fd fd) override;

    std::vector<std::string> mime_types;
    std::vector<PrimarySelectionDeviceController::Offer*> offers;
};

class Global : public PrimarySelectionDeviceManagerV1::Global, PrimarySelectionDeviceController
{
public:
    explicit Global(wl_display* display);

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
    struct wl_resource* resource, PrimarySelectionDeviceController* controller) :
    PrimarySelectionDeviceManagerV1(resource),
    controller{controller}
{
}

void PrimarySelectionDevice::set_selection(std::experimental::optional<struct wl_resource*> const& source, uint32_t /*serial*/)
{
    if (source)
    {
        controller->set_selection(dynamic_cast<PrimarySelectionDeviceController::Source*>(PrimarySelectionSource::from(source.value())));
    }
    else
    {
        controller->set_selection(&PrimarySelectionDeviceController::null_source);
    }
}

void PrimarySelectionDevice::destroy()
{
    controller->remove(this);
    destroy_wayland_object();
}

PrimarySelectionDevice::PrimarySelectionDevice(
    struct wl_resource* resource,
    PrimarySelectionDeviceController* controller) :
    PrimarySelectionDeviceV1(resource),
    controller{controller}
{
    controller->add(this);
}

void PrimarySelectionDevice::data_offer(PrimarySelectionDeviceController::Offer* offer)
{
    if (auto offer_resource = offer->resource())
        send_data_offer_event(offer_resource.value());
}

auto PrimarySelectionDevice::client() const -> wl_client*
{
    return PrimarySelectionDeviceV1::client;
}

auto PrimarySelectionDevice::resource() const -> wl_resource*
{
    return PrimarySelectionDeviceV1::resource;
}

void PrimarySelectionDevice::select(PrimarySelectionDeviceController::Offer* offer)
{
    send_selection_event(offer->resource());
}

PrimarySelectionOffer::PrimarySelectionOffer(
    struct wl_resource* resource,
    PrimarySelectionDeviceController::Source* source,
    PrimarySelectionDeviceController* controller) :
    PrimarySelectionOfferV1(resource),
    source{source},
    controller{controller}
{
}

auto PrimarySelectionOffer::resource() const -> std::experimental::optional<wl_resource*>
{
    return PrimarySelectionOfferV1::resource;
}

void PrimarySelectionOffer::offer(std::string const& mime_type)
{
    send_offer_event(mime_type);
}

void PrimarySelectionSource::offer(std::string const& mime_type)
{
    mime_types.push_back(mime_type);
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
    source = &PrimarySelectionDeviceController::null_source;
}

void PrimarySelectionSource::destroy()
{
    controller->set_selection(&PrimarySelectionDeviceController::null_source);
    destroy_wayland_object();
}

PrimarySelectionSource::PrimarySelectionSource(
    struct wl_resource* resource,
    PrimarySelectionDeviceController* controller) :
    PrimarySelectionSourceV1(resource),
    controller{controller}
{
}

void PrimarySelectionSource::cancelled()
{
    for (auto const offer : offers)
        offer->source_cancelled();

    send_cancelled_event();
}

void PrimarySelectionSource::create_offer_for(PrimarySelectionDeviceController::Device* device)
{
    wl_resource* new_resource = wl_resource_create(
        device->client(),
        &mir::wayland::zwp_primary_selection_offer_v1_interface_data,
        wl_resource_get_version(device->resource()),
        0);

    auto const offer = new PrimarySelectionOffer{new_resource, this, controller};
    device->data_offer(offer);

    for (auto const& mime_type : mime_types)
        offer->offer(mime_type);

    device->select(offer);
    
    offers.push_back(offer);
}

void PrimarySelectionSource::cancel(PrimarySelectionDeviceController::Offer* offer)
{
    offers.erase(std::remove(begin(offers), end(offers), offer), end(offers));
}

void PrimarySelectionSource::receive(std::string const& mime_type, mir::Fd fd)
{
    send_send_event(mime_type, fd);
}

Global::Global(wl_display* display) :
    PrimarySelectionDeviceManagerV1::Global(display, 1)
{
}

void Global::bind(wl_resource* new_zwp_primary_selection_device_manager_v1)
{
    new PrimarySelectionDeviceManager{new_zwp_primary_selection_device_manager_v1, this};
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
