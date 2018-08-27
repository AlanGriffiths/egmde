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
#include "printer.h"

#include <mir/client/display_config.h>
#include <mir/client/window_spec.h>

#include <mir_toolkit/mir_buffer_stream.h>

#include <algorithm>
#include <cstring>
#include <sstream>


using namespace mir::client;

namespace
{
void render_gradient(MirGraphicsRegion* region, uint8_t* bottom_colour, uint8_t* top_colour)
{
    char* row = region->vaddr;

    for (int j = 0; j < region->height; j++)
    {
        auto* pixel = (uint32_t*)row;
        uint8_t pattern_[4];
        for (auto i = 0; i != 3; ++i)
            pattern_[i] = (j*bottom_colour[i] + (region->height-j)*top_colour[i])/region->height;
        pattern_[3] = 0xff;

        for (int i = 0; i < region->width; i++)
            memcpy(pixel + i, pattern_, sizeof pixel[i]);

        row += region->stride;
    }

    static egmde::Printer printer;

    printer.footer(*region, {"Ctrl-Alt-A = app launcher | Ctrl-Alt-BkSp = quit"});
}
}

void egmde::Wallpaper::start(Connection connection)
{
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
        this->connection = std::move(connection);
    }

    enqueue_work([this]{ create_windows(); });
    start_work();
}

void egmde::Wallpaper::stop()
{
    {
        std::lock_guard<decltype(mutex)> lock{mutex};

        windows.clear();
        connection.reset();
    }
    stop_work();
}

void egmde::Wallpaper::handle_event(MirWindow* window, MirEvent const* event, void* context)
{
    static_cast<Wallpaper*>(context)->handle_event(window, event);
}

void egmde::Wallpaper::create_windows()
{
    DisplayConfig{connection}.for_each_output([this](MirOutput const* output)
    {
        if (!mir_output_is_enabled(output))
            return;

        auto const id = mir_output_get_id(output);
        auto const width = mir_output_get_logical_width(output);
        auto const height = mir_output_get_logical_height(output);

        std::lock_guard<decltype(mutex)> lock{mutex};

        Surface surface{mir_connection_create_render_surface_sync(connection, width, height)};

        auto buffer_stream = mir_render_surface_get_buffer_stream(surface, width, height, mir_pixel_format_xrgb_8888);

        auto window = WindowSpec::for_gloss(connection, width, height)
          .set_name("wallpaper")
          .set_fullscreen_on_output(id)
          .set_event_handler(&handle_event, this)
          .add_surface(surface, width, height, 0, 0)
          .create_window();

        MirGraphicsRegion graphics_region;

        mir_buffer_stream_get_graphics_region(buffer_stream, &graphics_region);

        render_gradient(&graphics_region, bottom_colour, top_colour);
        mir_buffer_stream_swap_buffers_sync(buffer_stream);

        windows.push_back({surface, buffer_stream, window});
    });
}

void egmde::Wallpaper::handle_event(MirWindow* window, MirEvent const* ev)
{
    switch (mir_event_get_type(ev))
    {
        case mir_event_type_resize:
        {
            MirResizeEvent const* resize = mir_event_get_resize_event(ev);
            int const new_width = mir_resize_event_get_width(resize);
            int const new_height = mir_resize_event_get_height(resize);

            enqueue_work([window, new_width, new_height, this]()
            {
                for (auto& w : windows)
                {
                    if (w.window != window)
                        continue;

                    mir_buffer_stream_set_size(w.buffer_stream, new_width, new_height);
                    mir_render_surface_set_size(w.surface, new_width, new_height);

                    WindowSpec::for_changes(connection)
                        .add_surface(w.surface, new_width, new_height, 0, 0)
                        .apply_to(window);

                    MirGraphicsRegion graphics_region;

                    // We expect a buffer of the right size so we shouldn't need to limit repaints
                    // but we also to avoid an infinite loop.
                    int repaint_limit = 3;

                    do
                    {
                        mir_buffer_stream_get_graphics_region(w.buffer_stream, &graphics_region);
                        render_gradient(&graphics_region, bottom_colour, top_colour);
                        mir_buffer_stream_swap_buffers_sync(w.buffer_stream);
                    }
                    while ((new_width != graphics_region.width || new_height != graphics_region.height)
                           && --repaint_limit != 0);
                    }
            });
            break;
        }

        default:
            break;
    }
}

void egmde::Wallpaper::bottom(std::string const& option)
{
    uint32_t value;
    std::stringstream interpreter{option};

    if (interpreter >> std::hex >> value)
    {
        bottom_colour[0] = value & 0xff;
        bottom_colour[1] = (value >> 8) & 0xff;
        bottom_colour[2] = (value >> 16) & 0xff;
    }
}

void egmde::Wallpaper::top(std::string const& option)
{
    uint32_t value;
    std::stringstream interpreter{option};

    if (interpreter >> std::hex >> value)
    {
        top_colour[0] = value & 0xff;
        top_colour[1] = (value >> 8) & 0xff;
        top_colour[2] = (value >> 16) & 0xff;
    }
}
