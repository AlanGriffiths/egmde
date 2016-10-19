/*
 * Copyright © 2016 Canonical Ltd.
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

#include "egwallpaper.h"

#include <miral/toolkit/surface_spec.h>

#include <mir_toolkit/mir_buffer_stream.h>

#include <cstring>


using namespace miral::toolkit;

namespace
{
void render_pattern(MirGraphicsRegion* region, uint8_t pattern[])
{
    char* row = region->vaddr;

    for (int j = 0; j < region->height; j++)
    {
        uint32_t* pixel = (uint32_t*)row;

        for (int i = 0; i < region->width; i++)
            memcpy(pixel + i, pattern, sizeof pixel[i]);

        row += region->stride;
    }
}
}

void Wallpaper::operator()(miral::toolkit::Connection connection)
{
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
        this->connection = connection;
    }

    enqueue_work([this]{ create_surface(); });
    start_work();
}

void Wallpaper::create_surface()
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    auto const spec = SurfaceSpec::for_normal_surface(
        connection, 100, 100, mir_pixel_format_xrgb_8888)
        .set_buffer_usage(mir_buffer_usage_software)
        .set_type(mir_surface_type_gloss)
        .set_name("wallpaper");

    mir_surface_spec_set_fullscreen_on_output(spec, 0);

    surface = spec.create_surface();

    uint8_t pattern[4] = { 0x14, 0x48, 0xDD, 0xFF };

    MirGraphicsRegion graphics_region;
    MirBufferStream* buffer_stream = mir_surface_get_buffer_stream(surface);

    mir_buffer_stream_get_graphics_region(buffer_stream, &graphics_region);

    render_pattern(&graphics_region, pattern);
    mir_buffer_stream_swap_buffers_sync(buffer_stream);
}

void Wallpaper::operator()(std::weak_ptr<mir::scene::Session> const& application)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    this->weak_session = application;
}

auto Wallpaper::application() const -> miral::Application
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    return weak_session.lock();
}

void Wallpaper::stop()
{
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
        surface.reset();
        connection.reset();
    }
    stop_work();
}

Worker::~Worker()
{
    if (worker.joinable()) worker.join();
}

void Worker::do_work()
{
    while (!work_done)
    {
        WorkQueue::value_type work;
        {
            std::unique_lock<decltype(work_mutex)> lock{work_mutex};
            work_cv.wait(lock, [this] { return !work_queue.empty(); });
            work = work_queue.front();
            work_queue.pop();
        }

        work();
    }
}

void Worker::enqueue_work(std::function<void()> const& functor)
{
    std::lock_guard<decltype(work_mutex)> lock{work_mutex};
    work_queue.push(functor);
    work_cv.notify_one();
}

void Worker::start_work()
{
    worker = std::thread{[this] { do_work(); }};
}

void Worker::stop_work()
{
    enqueue_work([this] { work_done = true; });
}
