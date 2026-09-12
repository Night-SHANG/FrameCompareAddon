#include "control/split_motion.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

bool near(float left, float right)
{
    return std::fabs(left - right) < 0.0001f;
}

void held_manual_input_moves_smoothly_and_stops_auto()
{
    framecompare::control::SplitMotionController controller;
    controller.settings().manual_speed = 0.25f;
    controller.settings().auto_active = true;

    float position = 0.5f;
    controller.update(position, 2.0f, false, true);

    require(position == 1.0f,
            "held right input should move by speed times frame time");
    require(!controller.settings().auto_active,
            "manual movement should stop automatic sweep");
}

void one_way_sweeps_start_at_an_edge_and_stop_at_the_other()
{
    framecompare::control::SplitMotionController controller;
    controller.settings().auto_speed = 0.5f;
    float position = 0.4f;

    controller.start(framecompare::control::SweepMode::left_to_right,
                     position);
    require(position == 0.0f, "left-to-right should start at the left edge");
    controller.update(position, 1.0f, false, false);
    require(position == 0.5f && controller.settings().auto_active,
            "left-to-right should advance while active");
    controller.update(position, 1.0f, false, false);
    require(position == 1.0f && !controller.settings().auto_active,
            "left-to-right should stop at the right edge");

    controller.start(framecompare::control::SweepMode::right_to_left,
                     position);
    require(position == 1.0f, "right-to-left should start at the right edge");
    controller.update(position, 2.0f, false, false);
    require(position == 0.0f && !controller.settings().auto_active,
            "right-to-left should stop at the left edge");
}

void ping_pong_reverses_at_both_edges()
{
    framecompare::control::SplitMotionController controller;
    controller.settings().auto_speed = 1.0f;
    float position = 0.7f;

    controller.start(framecompare::control::SweepMode::ping_pong, position);
    require(position == 0.5f, "ping-pong should begin at the center");
    controller.update(position, 0.75f, false, false);
    require(position == 0.75f && controller.settings().auto_active,
            "ping-pong should reflect overshoot at the right edge");
    controller.update(position, 1.0f, false, false);
    require(position == 0.25f,
            "ping-pong should reflect again at the left edge");
}

void ping_pong_stop_animates_back_to_the_center()
{
    framecompare::control::SplitMotionController controller;
    controller.settings().auto_speed = 0.2f;
    float position = 0.1f;

    controller.start(framecompare::control::SweepMode::ping_pong,
                     position);
    require(position == 0.5f,
            "ping-pong start should not jump from the center to an edge");
    controller.update(position, 1.0f, false, false);
    require(near(position, 0.7f) && controller.settings().auto_active,
            "sweep should be active away from the center before stopping");

    controller.toggle(position);
    require(position == 0.7f,
            "ping-pong stop should not jump immediately to the center");
    require(controller.settings().auto_active,
            "ping-pong should remain active while returning to center");
    controller.update(position, 0.5f, false, false);
    require(near(position, 0.6f) && controller.settings().auto_active,
            "return should use the configured automatic speed");
    controller.update(position, 0.5f, false, false);
    require(position == 0.5f && !controller.settings().auto_active,
            "return should stop exactly at the center");
}

void one_way_stop_still_returns_to_center_immediately()
{
    framecompare::control::SplitMotionController controller;
    controller.settings().auto_speed = 0.2f;
    float position = 0.5f;

    controller.start(framecompare::control::SweepMode::left_to_right,
                     position);
    controller.update(position, 1.0f, false, false);
    controller.toggle(position);

    require(position == 0.5f && !controller.settings().auto_active,
            "one-way sweep stop should retain immediate center reset");
}

void freeze_reuses_a_ready_pair_but_can_create_the_first_pair()
{
    require(!framecompare::control::should_capture_pair(true, true),
            "freeze should reuse an existing ready pair");
    require(framecompare::control::should_capture_pair(true, false),
            "freeze should still capture when no ready pair exists");
    require(framecompare::control::should_capture_pair(false, true),
            "realtime mode should keep capturing every frame");
}
}

int main()
{
    held_manual_input_moves_smoothly_and_stops_auto();
    one_way_sweeps_start_at_an_edge_and_stop_at_the_other();
    ping_pong_reverses_at_both_edges();
    ping_pong_stop_animates_back_to_the_center();
    one_way_stop_still_returns_to_center_immediately();
    freeze_reuses_a_ready_pair_but_can_create_the_first_pair();
    std::cout << "split_motion_tests: PASS\n";
}
