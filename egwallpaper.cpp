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

#include "egwallpaper.h"

#include <mir/client/display_config.h>
#include <mir/client/surface.h>
#include <mir/client/window_spec.h>

#include <mir_toolkit/mir_buffer_stream.h>

#include <algorithm>
#include <cstring>
#include <sstream>


using namespace mir::client;

namespace
{
void render_gradient(MirGraphicsRegion* region, uint8_t* colour)
{
    char* row = region->vaddr;

    for (int j = 0; j < region->height; j++)
    {
        auto* pixel = (uint32_t*)row;
        uint8_t pattern_[4];
        for (auto i = 0; i != 3; ++i)
            pattern_[i] = (j*colour[i])/region->height;
        pattern_[3] = colour[3];

        for (int i = 0; i < region->width; i++)
            memcpy(pixel + i, pattern_, sizeof pixel[i]);

        row += region->stride;
    }
}
}

void Wallpaper::start(Connection connection)
{
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
        this->connection = std::move(connection);
    }

    enqueue_work([this]{ create_window(); });
    start_work();
}

void Wallpaper::stop()
{
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
        window.reset();
        connection.reset();
    }
    stop_work();
}

void Wallpaper::create_window()
{
    unsigned width = 0;
    unsigned height = 0;

    DisplayConfig{connection}.for_each_output([&width, &height](MirOutput const* output)
    {
        if (!mir_output_is_enabled(output))
            return;

         width = std::max(width, mir_output_get_logical_width(output));
         height = std::max(height, mir_output_get_logical_height(output));
    });
    
    std::lock_guard<decltype(mutex)> lock{mutex};

    Surface surface{mir_connection_create_render_surface_sync(connection, width, height)};

    auto const buffer_stream =
        mir_render_surface_get_buffer_stream(surface, width, height, mir_pixel_format_xrgb_8888);

    window = WindowSpec::for_gloss(connection, width, height)
        .set_name("wallpaper")
        .set_fullscreen_on_output(0)
        .add_surface(surface, width, height, 0, 0)
        .create_window();

    MirGraphicsRegion graphics_region;

    mir_buffer_stream_get_graphics_region(buffer_stream, &graphics_region);

    render_gradient(&graphics_region, colour);
    mir_buffer_stream_swap_buffers_sync(buffer_stream);
}

void Wallpaper::operator()(std::string const& option)
{
    uint32_t value;
    std::stringstream interpreter{option};

    if (interpreter >> std::hex >> value)
    {
        colour[0] = value & 0xff;
        colour[1] = (value >> 8) & 0xff;
        colour[2] = (value >> 16) & 0xff;
    }
}


Worker::~Worker() = default;

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
    do_work();
}

void Worker::stop_work()
{
    enqueue_work([this] { work_done = true; });
}
