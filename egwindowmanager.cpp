/*
 * Copyright © 2016-18 Octopull Ltd.
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

#include "egwindowmanager.h"

#include <miral/window_info.h>
#include <miral/window_manager_tools.h>

#include <linux/input.h>

using namespace mir::geometry;

namespace
{
unsigned int const shift_states =
    mir_input_event_modifier_alt |
    mir_input_event_modifier_shift |
    mir_input_event_modifier_sym |
    mir_input_event_modifier_ctrl |
    mir_input_event_modifier_meta;
}

bool egmde::WindowManagerPolicy::handle_pointer_event(MirPointerEvent const* event)
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

    bool consumes_event = false;

    switch (pointer_gesture)
    {
        case pointer_gesture_moving:
            if (action == mir_pointer_action_motion &&
                shift_state == pointer_gesture_shift_keys &&
                mir_pointer_event_button_state(event, pointer_gesture_button))
            {
                if (pointer_gesture_window &&
                    tools.select_active_window(pointer_gesture_window) == pointer_gesture_window)
                {
                    tools.drag_active_window(movement);
                }
                else
                {
                    pointer_gesture = pointer_gesture_none;
                }

                consumes_event = true;
            }
            else
            {
                pointer_gesture = pointer_gesture_none;
            }
            break;

        case pointer_gesture_resizing:
            if (action == mir_pointer_action_motion &&
                shift_state == pointer_gesture_shift_keys &&
                mir_pointer_event_button_state(event, pointer_gesture_button))
            {
                if (pointer_gesture_window &&
                    tools.select_active_window(pointer_gesture_window) == pointer_gesture_window)
                {
                    auto const top_left = resize_top_left;
                    Rectangle const old_pos{top_left, resize_size};

                    auto new_width = old_pos.size.width;
                    auto new_height = old_pos.size.height;

                    if (resize_edge & mir_resize_edge_east)
                        new_width = old_pos.size.width + movement.dx;

                    if (resize_edge & mir_resize_edge_west)
                        new_width = old_pos.size.width - movement.dx;

                    if (resize_edge & mir_resize_edge_north)
                        new_height = old_pos.size.height - movement.dy;

                    if (resize_edge & mir_resize_edge_south)
                        new_height = old_pos.size.height + movement.dy;

                    keep_size_within_limits(tools.info_for(pointer_gesture_window), movement, new_width, new_height);

                    Size new_size{new_width, new_height};

                    Point new_pos = top_left;

                    if (resize_edge & mir_resize_edge_west)
                        new_pos.x = top_left.x + movement.dx;

                    if (resize_edge & mir_resize_edge_north)
                        new_pos.y = top_left.y + movement.dy;

                    WindowSpecification modifications;
                    modifications.top_left() = new_pos;
                    modifications.size() = new_size;
                    tools.modify_window(pointer_gesture_window, modifications);
                    resize_top_left = new_pos;
                    resize_size = new_size;
                }
                else
                {
                    pointer_gesture = pointer_gesture_none;
                }

                consumes_event = true;
            }
            else
            {
                pointer_gesture = pointer_gesture_none;
            }
            break;

        default:
            break;
    }

    if (!consumes_event)
    {
        switch (action)
        {
            case mir_pointer_action_button_down:
                if (auto const window = tools.window_at(cursor))
                    tools.select_active_window(window);
                break;

            case mir_pointer_action_motion:
                if (shift_state == mir_input_event_modifier_alt&&
                    mir_pointer_event_button_state(event, mir_pointer_button_primary))
                {
                    if (auto const target = tools.window_at(cursor - movement))
                    {
                        if (tools.select_active_window(target) == target)
                            tools.drag_active_window(movement);
                    }
                    consumes_event = true;
                }
                break;

            default:
                break;
        }
    }

    return consumes_event;
}

bool egmde::WindowManagerPolicy::handle_touch_event(MirTouchEvent const* event)
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
                // Falls through
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
        end_touch_gesture();

    old_touch_pinch_top = touch_pinch_top;
    old_touch_pinch_left = touch_pinch_left;
    old_touch_pinch_width = touch_pinch_width;
    old_touch_pinch_height = touch_pinch_height;
    return consumes_event;
}

bool egmde::WindowManagerPolicy::handle_keyboard_event(MirKeyboardEvent const* event)
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

            default:;
        }
    }

    return false;
}

void egmde::WindowManagerPolicy::end_touch_gesture()
{
    if (!pinching)
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

    pinching = false;
}

void egmde::WindowManagerPolicy::keep_size_within_limits(
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

Rectangle egmde::WindowManagerPolicy::confirm_placement_on_display(
    WindowInfo const& /*window_info*/, MirWindowState /*new_state*/, Rectangle const& new_placement)
{
    return new_placement;
}

void egmde::WindowManagerPolicy::handle_request_drag_and_drop(WindowInfo& /*window_info*/)
{
}

void egmde::WindowManagerPolicy::handle_request_move(WindowInfo& window_info, MirInputEvent const* input_event)
{
    begin_pointer_gesture(window_info, input_event, pointer_gesture_moving);
}

void egmde::WindowManagerPolicy::handle_request_resize(
    WindowInfo& window_info, MirInputEvent const* input_event, MirResizeEdge edge)
{
    if (begin_pointer_gesture(window_info, input_event, pointer_gesture_resizing))
    {
        resize_edge = edge;
        resize_top_left = pointer_gesture_window.top_left();
        resize_size = pointer_gesture_window.size();
    }
}

bool egmde::WindowManagerPolicy::begin_pointer_gesture(
    WindowInfo const& window_info,
    MirInputEvent const* input_event,
    PointerGesture gesture)
{
    if (mir_input_event_get_type(input_event) != mir_input_event_type_pointer)
        return false;

    MirPointerEvent const* const pointer_event = mir_input_event_get_pointer_event(input_event);
    pointer_gesture = gesture;
    pointer_gesture_window = window_info.window();
    pointer_gesture_shift_keys = mir_pointer_event_modifiers(pointer_event) & shift_states;

    for (auto button : {mir_pointer_button_primary, mir_pointer_button_secondary, mir_pointer_button_tertiary})
    {
        if (mir_pointer_event_button_state(pointer_event, button))
        {
            pointer_gesture_button = button;
            break;
        }
    }

    return true;
}
