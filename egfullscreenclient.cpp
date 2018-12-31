/*
 * Copyright © 2018 Octopull Ltd.
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

#include "egfullscreenclient.h"

#include <wayland-client.h>

#include <boost/throw_exception.hpp>

#include <fcntl.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/poll.h>

#include <cstring>
#include <system_error>

void egmde::FullscreenClient::Output::geometry(
    void* data,
    struct wl_output* /*wl_output*/,
    int32_t x,
    int32_t y,
    int32_t /*physical_width*/,
    int32_t /*physical_height*/,
    int32_t /*subpixel*/,
    const char */*make*/,
    const char */*model*/,
    int32_t transform)
{
    auto output = static_cast<Output*>(data);

    output->x = x;
    output->y = y;
    output->transform = transform;
}


void egmde::FullscreenClient::Output::mode(
    void *data,
    struct wl_output* /*wl_output*/,
    uint32_t flags,
    int32_t width,
    int32_t height,
    int32_t /*refresh*/)
{
    if (!(WL_OUTPUT_MODE_CURRENT & flags))
        return;

    auto output = static_cast<Output*>(data);

    output->width = width,
        output->height = height;
}

void egmde::FullscreenClient::Output::scale(void* /*data*/, wl_output* /*wl_output*/, int32_t /*factor*/)
{
}

egmde::FullscreenClient::Output::Output(
    wl_output* output,
    std::function<void(Output const&)> on_constructed,
    std::function<void(Output const&)> on_change)
    : output{output},
      on_done{[this, on_constructed = std::move(on_constructed), on_change=std::move(on_change)]
      (Output const& o) mutable { on_constructed(o), on_done = std::move(on_change); }}
{
    wl_output_add_listener(output, &output_listener, this);
}

egmde::FullscreenClient::Output::~Output()
{
    if (output)
        wl_output_destroy(output);
}

wl_output_listener const egmde::FullscreenClient::Output::output_listener = {
    &geometry,
    &mode,
    &done,
    &scale,
};

void egmde::FullscreenClient::Output::done(void* data, struct wl_output* /*wl_output*/)
{
    auto output = static_cast<Output*>(data);
    output->on_done(*output);
}

egmde::FullscreenClient::FullscreenClient(wl_display* display) :
    shutdown_signal{::eventfd(0, EFD_CLOEXEC)},
    registry{nullptr, [](auto){}}
{
    if (shutdown_signal == mir::Fd::invalid)
    {
        BOOST_THROW_EXCEPTION((std::system_error{errno, std::system_category(), "Failed to create shutdown notifier"}));
    }

    this->display = display;

    registry = {wl_display_get_registry(display), &wl_registry_destroy};

    static wl_registry_listener const registry_listener = {
        new_global,
        remove_global
    };

    wl_registry_add_listener(registry.get(), &registry_listener, this);
    wl_display_roundtrip(display);
    wl_display_roundtrip(display);
}

void egmde::FullscreenClient::on_output_changed(Output const* output)
{
    std::lock_guard<decltype(outputs_mutex)> lock{outputs_mutex};
    auto const p = outputs.find(output);
    if (p != end(outputs))
        draw_screen(p->second);
}

void egmde::FullscreenClient::on_output_gone(Output const* output)
{
    std::lock_guard<decltype(outputs_mutex)> lock{outputs_mutex};
    outputs.erase(output);
}

void egmde::FullscreenClient::on_new_output(Output const* output)
{
    std::lock_guard<decltype(outputs_mutex)> lock{outputs_mutex};
    draw_screen(outputs.insert({output, SurfaceInfo{output}}).first->second);
}

auto egmde::FullscreenClient::make_shm_pool(int size, void **data)
-> std::unique_ptr<wl_shm_pool, void(*)(wl_shm_pool*)>
{
    mir::Fd fd{open("/dev/shm", O_TMPFILE | O_RDWR | O_EXCL, S_IRWXU)};
    if (fd < 0) {
        BOOST_THROW_EXCEPTION((std::system_error{errno, std::system_category(), "Failed to open shm buffer"}));
    }

    if (auto error = posix_fallocate(fd, 0, size))
    {
        BOOST_THROW_EXCEPTION((std::system_error{error, std::system_category(), "Failed to allocate shm buffer"}));
    }

    if ((*data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED)
    {
        BOOST_THROW_EXCEPTION((std::system_error{errno, std::system_category(), "Failed to mmap buffer"}));
    }

    return {wl_shm_create_pool(shm, fd, size),&wl_shm_pool_destroy};
}

egmde::FullscreenClient::~FullscreenClient()
{
    {
        std::lock_guard<decltype(outputs_mutex)> lock{outputs_mutex};
        outputs.clear();
    }
    bound_outputs.clear();
    registry.reset();
    wl_display_roundtrip(display);
}

void egmde::FullscreenClient::new_global(
    void* data,
    struct wl_registry* registry,
    uint32_t id,
    char const* interface,
    uint32_t version)
{
    (void)version;
    FullscreenClient* self = static_cast<decltype(self)>(data);

    if (strcmp(interface, "wl_compositor") == 0)
    {
        self->compositor =
            static_cast<decltype(self->compositor)>(wl_registry_bind(registry, id, &wl_compositor_interface, 3));
    }
    else if (strcmp(interface, "wl_shm") == 0)
    {
        self->shm = static_cast<decltype(self->shm)>(wl_registry_bind(registry, id, &wl_shm_interface, 1));
        // Normally we'd add a listener to pick up the supported formats here
        // As luck would have it, I know that argb8888 is the only format we support :)
    }
    else if (strcmp(interface, "wl_seat") == 0)
    {
        self->seat = static_cast<decltype(self->seat)>(wl_registry_bind(registry, id, &wl_seat_interface, 4));
    }
    else if (strcmp(interface, "wl_output") == 0)
    {
        // NOTE: We'd normally need to do std::min(version, 2), lest the compositor only support version 1
        // of the interface. However, we're an internal client of a compositor that supports version 2, so…
        auto output =
            static_cast<wl_output*>(wl_registry_bind(registry, id, &wl_output_interface, 2));
        self->bound_outputs.insert(
            std::make_pair(
                id,
                std::make_unique<Output>(
                    output,
                    [self](Output const& output) { self->on_new_output(&output); },
                    [self](Output const& output) { self->on_output_changed(&output); })));
    }
    else if (strcmp(interface, "wl_shell") == 0)
    {
        self->shell = static_cast<decltype(self->shell)>(wl_registry_bind(registry, id, &wl_shell_interface, 1));
    }
}

void egmde::FullscreenClient::remove_global(
    void* data,
    struct wl_registry* /*registry*/,
    uint32_t id)
{
    FullscreenClient* self = static_cast<decltype(self)>(data);

    auto const output = self->bound_outputs.find(id);
    if (output != self->bound_outputs.end())
    {
        self->on_output_gone(output->second.get());
        self->bound_outputs.erase(output);
    }
    // TODO: We should probably also delete any other globals we've bound to that disappear.
}

void egmde::FullscreenClient::run(wl_display* display)
{
    enum FdIndices {
        display_fd = 0,
        shutdown,
        indices
    };

    pollfd fds[indices] =
        {
            fds[display_fd] = {wl_display_get_fd(display), POLLIN, 0},
            {shutdown_signal, POLLIN, 0},
        };

    while (!(fds[shutdown].revents & (POLLIN | POLLERR)))
    {
        while (wl_display_prepare_read(display) != 0)
        {
            if (wl_display_dispatch_pending(display) == -1)
            {
                BOOST_THROW_EXCEPTION((std::system_error{errno, std::system_category(), "Failed to dispatch Wayland events"}));
            }
        }

        if (poll(fds, indices, -1) == -1)
        {
            wl_display_cancel_read(display);
            BOOST_THROW_EXCEPTION((std::system_error{errno, std::system_category(), "Failed to wait for event"}));
        }

        if (fds[display_fd].revents & (POLLIN | POLLERR))
        {
            if (wl_display_read_events(display))
            {
                BOOST_THROW_EXCEPTION((std::system_error{errno, std::system_category(), "Failed to read Wayland events"}));
            }
        }
        else
        {
            wl_display_cancel_read(display);
        }
    }
}

void egmde::FullscreenClient::stop()
{
    if (eventfd_write(shutdown_signal, 1) == -1)
    {
        BOOST_THROW_EXCEPTION((std::system_error{errno, std::system_category(), "Failed to shutdown internal client"}));
    }
}

void egmde::FullscreenClient::for_each_surface(std::function<void(SurfaceInfo&)> const& f) const
{
    std::lock_guard<decltype(outputs_mutex)> lock{outputs_mutex};
    for (auto& os : outputs)
    {
        f(const_cast<SurfaceInfo&>(os.second));
    }
}
