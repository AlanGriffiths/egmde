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

#ifndef EGMDE_EGPRIMARY_SELECTION_DEVICE_CONTROLLER_H
#define EGMDE_EGPRIMARY_SELECTION_DEVICE_CONTROLLER_H

#include <mir/fd.h>
#include <wayland-server-core.h>

#include <experimental/optional>
#include <string>
#include <vector>

namespace egmde
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

        void make_data_offer(Offer* offer);

        virtual void send_data_offer(wl_resource* resource) const = 0;

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

        void disclose(Device* device, Offer* offer);
        void add_mime_type(std::string const& mime_type);
        void cancel_offer(Offer* offer);
        void cancel_offers();

    private:
        std::vector<std::string> mime_types;
        std::vector<Offer*> offers;
    };

    void set_selection(Source* source);

    void add(Device* device);

    void remove(Device* device);

    static Source* const null_source;

private:
    Source* current_selection = null_source;

    std::vector<Device*> devices;
};
}

#endif //EGMDE_EGPRIMARY_SELECTION_DEVICE_CONTROLLER_H
