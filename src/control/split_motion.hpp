#pragma once

#include <cstdint>

namespace framecompare::control
{
enum class SweepMode : std::uint8_t
{
    left_to_right,
    right_to_left,
    ping_pong,
};

struct MotionSettings
{
    float manual_speed = 0.25f;
    float auto_speed = 0.20f;
    SweepMode sweep_mode = SweepMode::left_to_right;
    bool auto_active = false;
    bool frozen = false;
};

class SplitMotionController
{
public:
    MotionSettings &settings() noexcept;
    const MotionSettings &settings() const noexcept;

    void start(SweepMode mode, float &position) noexcept;
    void stop(float &position) noexcept;
    void toggle(float &position) noexcept;
    void update(float &position, float delta_seconds,
                bool move_left, bool move_right) noexcept;

private:
    MotionSettings settings_;
    int ping_pong_direction_ = 1;
};

SplitMotionController &split_motion() noexcept;
bool should_capture_pair(bool frozen, bool pair_ready) noexcept;
}
