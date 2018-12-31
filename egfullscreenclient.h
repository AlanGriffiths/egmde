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

#ifndef EGMDE_EGFULLSCREENCLIENT_H
#define EGMDE_EGFULLSCREENCLIENT_H

#include <mir/fd.h>

#include <wayland-client.h>

#include <functional>
#include <map>
#include <memory>
#include <mutex>

namespace egmde
{
class FullscreenClient
{
public:
    FullscreenClient(wl_display* display);

    virtual ~FullscreenClient();

    void run(wl_display* display);

    void stop();

    auto make_shm_pool(int size, void** data)
    -> std::unique_ptr<wl_shm_pool, void (*)(wl_shm_pool*)>;

    wl_display* display = nullptr;
    wl_compositor* compositor = nullptr;
    wl_shell* shell = nullptr;

    class Output
    {
    public:
        Output(
            wl_output* output,
            std::function<void(Output const&)> on_constructed,
            std::function<void(Output const&)> on_change);

        ~Output();

        Output(Output const&) = delete;

        Output(Output&&) = delete;

        Output& operator=(Output const&) = delete;

        Output& operator=(Output&&) = delete;

        int32_t x;
        int32_t y;
        int32_t width;
        int32_t height;
        int32_t transform;
        wl_output* output;
    private:
        static void done(void* data, wl_output* output);

        static void geometry(
            void* data,
            wl_output* wl_output,
            int32_t x,
            int32_t y,
            int32_t physical_width,
            int32_t physical_height,
            int32_t subpixel,
            const char* make,
            const char* model,
            int32_t transform);

        static void mode(
            void* data,
            wl_output* wl_output,
            uint32_t flags,
            int32_t width,
            int32_t height,
            int32_t refresh);

        static void scale(void* data, wl_output* wl_output, int32_t factor);

        static wl_output_listener const output_listener;

        std::function<void(Output const&)> on_done;
    };

    struct SurfaceInfo
    {
        SurfaceInfo(Output const* output) :
            output{output} {}

        ~SurfaceInfo()
        {
            clear_window();
        }

        void clear_window()
        {
            if (buffer)
                wl_buffer_destroy(buffer);

            if (shell_surface)
                wl_shell_surface_destroy(shell_surface);

            if (surface)
                wl_surface_destroy(surface);


            buffer = nullptr;
            shell_surface = nullptr;
            surface = nullptr;
        }

        // Screen description
        Output const* output;

        // Content
        void* content_area = nullptr;
        wl_surface* surface = nullptr;
        wl_shell_surface* shell_surface = nullptr;
        wl_buffer* buffer = nullptr;
    };

    virtual void draw_screen(SurfaceInfo& info) const = 0;

    void for_each_surface(std::function<void(SurfaceInfo&)> const& f) const;

private:
    void on_new_output(Output const*);

    void on_output_changed(Output const*);

    void on_output_gone(Output const*);

    mir::Fd const shutdown_signal;

    std::mutex mutable outputs_mutex;
    std::map<Output const*, SurfaceInfo> outputs;

    wl_seat* seat = nullptr;
    wl_shm* shm = nullptr;

    static void new_global(
        void* data,
        struct wl_registry* registry,
        uint32_t id,
        char const* interface,
        uint32_t version);

    static void remove_global(
        void* data,
        struct wl_registry* registry,
        uint32_t name);

    std::unique_ptr<wl_registry, decltype(&wl_registry_destroy)> registry;

    std::unordered_map<uint32_t, std::unique_ptr<Output>> bound_outputs;
};
}

#endif //EGMDE_EGFULLSCREENCLIENT_H
