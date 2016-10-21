/*
 * Copyright © 2016 Octopull Ltd.
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

#include <miral/canonical_window_manager.h>
#include <miral/window_info.h>
#include <miral/window_manager_tools.h>

#include <miral/internal_client.h>
#include <miral/runner.h>
#include <miral/set_window_managment_policy.h>

#include <linux/input.h>

#include <limits>

using namespace miral;
using namespace mir::geometry;

namespace
{
class ExampleWindowManagerPolicy : public CanonicalWindowManagerPolicy
{
public:
    using CanonicalWindowManagerPolicy::CanonicalWindowManagerPolicy;

    // Switch apps  : Alt+Tab
    // Switch window: Alt+`
    // Close window : Alt-F4
    bool handle_keyboard_event(MirKeyboardEvent const* event) override;

    // Switch apps  : click on the corresponding window
    // Switch window: click on the corresponding window
    // Move window  : Alt-leftmousebutton drag
    // Resize window: Alt-middle_button drag
    bool handle_pointer_event(MirPointerEvent const* event) override;

    // Switch apps  : tap on the corresponding window
    // Switch window: tap on the corresponding window
    // Move window  : three finger drag
    // Resize window: three finger pinch
    bool handle_touch_event(MirTouchEvent const* event) override;

    void on_stop();

private:
    void pointer_resize(Window const& window, Point cursor, Point old_cursor);

    // State held for move/resize gesture by pointer
    bool pointer_resizing = false;
    bool is_left_resize = false;
    bool is_top_resize = false;

    // State held for move/resize gesture by touch
    int old_touch_pinch_top = 0;
    int old_touch_pinch_left = 0;
    int old_touch_pinch_width = 0;
    int old_touch_pinch_height = 0;
    bool pinching = false;

    void end_gesture();
    void keep_size_within_limits(
        WindowInfo const& window_info, Displacement& delta, Width& new_width, Height& new_height) const;
};
}

int main(int argc, char const* argv[])
{
    miral::MirRunner runner{argc, argv};

    Wallpaper wallpaper;

    runner.add_stop_callback([&] { wallpaper.stop(); });

    return runner.run_with(
        {
            miral::StartupInternalClient{"wallpaper", std::ref(wallpaper)},
            set_window_managment_policy<ExampleWindowManagerPolicy>()
        });
}

namespace
{
int const shift_states =
    mir_input_event_modifier_alt |
    mir_input_event_modifier_shift |
    mir_input_event_modifier_sym |
    mir_input_event_modifier_ctrl |
    mir_input_event_modifier_meta;

Width const min_width{5};
Height const min_height{5};
DeltaX const zero_dx{0};
DeltaY const zero_dy{0};
}

bool ExampleWindowManagerPolicy::handle_pointer_event(MirPointerEvent const* event)
{
    auto const action = mir_pointer_event_action(event);
    auto const shift_state = mir_pointer_event_modifiers(event) & shift_states;
    Point const cursor{
        mir_pointer_event_axis_value(event, mir_pointer_axis_x),
        mir_pointer_event_axis_value(event, mir_pointer_axis_y)
    };

    Displacement movement{
        mir_pointer_event_axis_value(event, mir_pointer_axis_relative_x),
        mir_pointer_event_axis_value(event, mir_pointer_axis_relative_y)
    };

    auto old_cursor = cursor - movement;

    bool consumes_event = false;
    bool is_resize_event = false;

    switch (action)
    {
    case mir_pointer_action_button_down:
        if (auto const window = tools.window_at(cursor))
            tools.select_active_window(window);
        break;

    case mir_pointer_action_motion:
        if (shift_state == mir_input_event_modifier_alt)
        {
            if (mir_pointer_event_button_state(event, mir_pointer_button_primary))
            {
                if (auto const target = tools.window_at(old_cursor))
                {
                    if (tools.select_active_window(target) == target)
                        tools.drag_active_window(movement);
                }
                consumes_event = true;
            }
            else if (mir_pointer_event_button_state(event, mir_pointer_button_tertiary))
            {
                if (auto const target = tools.window_at(old_cursor))
                {
                    if (!pointer_resizing)
                        is_resize_event = tools.select_active_window(target) == target;
                    else
                        is_resize_event = true;

                    if (is_resize_event)
                        pointer_resize(target, cursor, old_cursor);
                }

                consumes_event = true;
            }
        }
        break;

    default:
        break;
    }

    if (pointer_resizing && !is_resize_event)
        end_gesture();

    pointer_resizing = is_resize_event;
    old_cursor = cursor;
    return consumes_event;
}

bool ExampleWindowManagerPolicy::handle_touch_event(MirTouchEvent const* event)
{
    auto const count = mir_touch_event_point_count(event);

    long total_x = 0;
    long total_y = 0;

    for (auto i = 0U; i != count; ++i)
    {
        total_x += mir_touch_event_axis_value(event, i, mir_touch_axis_x);
        total_y += mir_touch_event_axis_value(event, i, mir_touch_axis_y);
    }

    Point cursor{total_x/count, total_y/count};

    bool is_drag = true;
    for (auto i = 0U; i != count; ++i)
    {
        switch (mir_touch_event_action(event, i))
        {
        case mir_touch_action_up:
            return false;

        case mir_touch_action_down:
            is_drag = false;

        default:
            continue;
        }
    }

    int touch_pinch_top = std::numeric_limits<int>::max();
    int touch_pinch_left = std::numeric_limits<int>::max();
    int touch_pinch_width = 0;
    int touch_pinch_height = 0;

    for (auto i = 0U; i != count; ++i)
    {
        for (auto j = 0U; j != i; ++j)
        {
            int dx = mir_touch_event_axis_value(event, i, mir_touch_axis_x) -
                     mir_touch_event_axis_value(event, j, mir_touch_axis_x);

            int dy = mir_touch_event_axis_value(event, i, mir_touch_axis_y) -
                     mir_touch_event_axis_value(event, j, mir_touch_axis_y);

            if (touch_pinch_width < dx)
                touch_pinch_width = dx;

            if (touch_pinch_height < dy)
                touch_pinch_height = dy;
        }

        int const x = mir_touch_event_axis_value(event, i, mir_touch_axis_x);

        int const y = mir_touch_event_axis_value(event, i, mir_touch_axis_y);

        if (touch_pinch_top > y)
            touch_pinch_top = y;

        if (touch_pinch_left > x)
            touch_pinch_left = x;
    }

    bool consumes_event = false;
    if (is_drag)
    {
        if (count == 3)
        {
            if (auto window = tools.active_window())
            {
                auto const old_size = window.size();
                auto const delta_width = DeltaX{touch_pinch_width - old_touch_pinch_width};
                auto const delta_height = DeltaY{touch_pinch_height - old_touch_pinch_height};

                auto new_width = std::max(old_size.width + delta_width, Width{5});
                auto new_height = std::max(old_size.height + delta_height, Height{5});
                Displacement delta{
                    DeltaX{touch_pinch_left - old_touch_pinch_left},
                    DeltaY{touch_pinch_top  - old_touch_pinch_top}};

                auto& window_info = tools.info_for(window);
                keep_size_within_limits(window_info, delta, new_width, new_height);

                auto new_pos = window.top_left() + delta;
                Size new_size{new_width, new_height};

                WindowSpecification modifications;
                modifications.top_left() = new_pos;
                modifications.size() = new_size;
                tools.modify_window(window_info, modifications);
                pinching = true;
            }
            consumes_event = true;
        }
    }
    else
    {
        if (auto const& window = tools.window_at(cursor))
            tools.select_active_window(window);
    }

    if (!consumes_event && pinching)
        end_gesture();

    old_touch_pinch_top = touch_pinch_top;
    old_touch_pinch_left = touch_pinch_left;
    old_touch_pinch_width = touch_pinch_width;
    old_touch_pinch_height = touch_pinch_height;
    return consumes_event;
}

bool ExampleWindowManagerPolicy::handle_keyboard_event(MirKeyboardEvent const* event)
{
    auto const action = mir_keyboard_event_action(event);
    auto const shift_state = mir_keyboard_event_modifiers(event) & shift_states;

    if (action == mir_keyboard_action_down && shift_state == mir_input_event_modifier_alt)
    {
        switch (mir_keyboard_event_scan_code(event))
        {
        case KEY_F4:
            tools.ask_client_to_close(tools.active_window());
            return true;

        case KEY_TAB:
            tools.focus_next_application();
            return true;

        case KEY_GRAVE:
            tools.focus_next_within_application();
            return true;
        }
    }

    return false;
}

void ExampleWindowManagerPolicy::pointer_resize(Window const& window, Point cursor, Point old_cursor)
{
    auto& window_info = tools.info_for(window);

    auto const top_left = window.top_left();
    Rectangle const old_pos{top_left, window.size()};

    if (!pointer_resizing)
    {
        auto anchor = old_pos.bottom_right();

        for (auto const& corner : {
            old_pos.top_right(),
            old_pos.bottom_left(),
            top_left})
        {
            if ((old_cursor - anchor).length_squared() <
                (old_cursor - corner).length_squared())
            {
                anchor = corner;
            }
        }

        is_left_resize = anchor.x != top_left.x;
        is_top_resize  = anchor.y != top_left.y;
    }

    auto movement = cursor-old_cursor;

    auto new_width  = old_pos.size.width  + (is_left_resize? -1 : 1) * movement.dx;
    auto new_height = old_pos.size.height + (is_top_resize ? -1 : 1) * movement.dy;

    if (new_width < min_width)
    {
        new_width = min_width;
        if (movement.dx > zero_dx)
            movement.dx = zero_dx;
    }

    if (new_height < min_height)
    {
        new_height = min_height;
        if (movement.dy > zero_dy)
            movement.dy = zero_dy;
    }

    if (!is_left_resize)
        movement.dx = zero_dx;

    if (!is_top_resize)
        movement.dy = zero_dy;

    Point new_pos = top_left + movement;
    Size new_size = {new_width, new_height};

    keep_size_within_limits(window_info, movement, new_width, new_height);
    WindowSpecification modifications;
    modifications.top_left() = new_pos;
    modifications.size() = new_size;
    tools.modify_window(window_info, modifications);
}

void ExampleWindowManagerPolicy::end_gesture()
{
    if (!pointer_resizing  && !pinching)
        return;

    if (auto window = tools.active_window())
    {
        auto& window_info = tools.info_for(window);

        auto new_size = window.size();
        auto new_pos  = window.top_left();
        window_info.constrain_resize(new_pos, new_size);

        WindowSpecification modifications;
        modifications.top_left() = new_pos;
        modifications.size() = new_size;
        tools.modify_window(window_info, modifications);
    }

    pointer_resizing = false;
    pinching = false;
}

void ExampleWindowManagerPolicy::keep_size_within_limits(
    WindowInfo const& window_info, Displacement& delta, Width& new_width, Height& new_height) const
{
    auto const min_width  = std::max(window_info.min_width(), Width{5});
    auto const min_height = std::max(window_info.min_height(), Height{5});

    if (new_width < min_width)
    {
        new_width = min_width;
        if (delta.dx > DeltaX{0})
            delta.dx = DeltaX{0};
    }

    if (new_height < min_height)
    {
        new_height = min_height;
        if (delta.dy > DeltaY{0})
            delta.dy = DeltaY{0};
    }

    auto const max_width  = window_info.max_width();
    auto const max_height = window_info.max_height();

    if (new_width > max_width)
    {
        new_width = max_width;
        if (delta.dx < DeltaX{0})
            delta.dx = DeltaX{0};
    }

    if (new_height > max_height)
    {
        new_height = max_height;
        if (delta.dy < DeltaY{0})
            delta.dy = DeltaY{0};
    }
}
