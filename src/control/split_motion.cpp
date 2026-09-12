#include "control/split_motion.hpp"

#include <algorithm>

namespace framecompare::control
{
namespace
{
SplitMotionController g_split_motion;
}

MotionSettings &SplitMotionController::settings() noexcept
{
    return settings_;
}

const MotionSettings &SplitMotionController::settings() const noexcept
{
    return settings_;
}

void SplitMotionController::start(SweepMode mode, float &position) noexcept
{
    settings_.sweep_mode = mode;
    settings_.auto_active = true;
    ping_pong_direction_ = 1;
    returning_to_center_ = false;

    if (mode == SweepMode::right_to_left)
        position = 1.0f;
    else if (mode == SweepMode::ping_pong)
        position = 0.5f;
    else
        position = 0.0f;
}

void SplitMotionController::stop(float &position) noexcept
{
    position = std::clamp(position, 0.0f, 1.0f);
    if (settings_.sweep_mode == SweepMode::ping_pong &&
        settings_.auto_active && position != 0.5f)
    {
        returning_to_center_ = true;
        return;
    }

    settings_.auto_active = false;
    returning_to_center_ = false;
    position = 0.5f;
}

void SplitMotionController::toggle(float &position) noexcept
{
    if (settings_.auto_active)
        stop(position);
    else
        start(settings_.sweep_mode, position);
}

void SplitMotionController::update(float &position, float delta_seconds,
                                   bool move_left, bool move_right) noexcept
{
    position = std::clamp(position, 0.0f, 1.0f);
    const float delta = std::max(delta_seconds, 0.0f);

    if (move_left || move_right)
    {
        settings_.auto_active = false;
        returning_to_center_ = false;
        const int direction = static_cast<int>(move_right) -
                              static_cast<int>(move_left);
        const float speed = std::max(settings_.manual_speed, 0.0f);
        position = std::clamp(
            position + static_cast<float>(direction) * speed * delta,
            0.0f, 1.0f);
        return;
    }

    if (!settings_.auto_active)
        return;

    const float step = std::max(settings_.auto_speed, 0.0f) * delta;
    if (returning_to_center_)
    {
        position = position < 0.5f
            ? std::min(position + step, 0.5f)
            : std::max(position - step, 0.5f);
        if (position == 0.5f)
        {
            settings_.auto_active = false;
            returning_to_center_ = false;
        }
        return;
    }

    switch (settings_.sweep_mode)
    {
    case SweepMode::left_to_right:
        position = std::min(position + step, 1.0f);
        if (position >= 1.0f)
            settings_.auto_active = false;
        break;
    case SweepMode::right_to_left:
        position = std::max(position - step, 0.0f);
        if (position <= 0.0f)
            settings_.auto_active = false;
        break;
    case SweepMode::ping_pong:
        position += static_cast<float>(ping_pong_direction_) * step;
        while (position > 1.0f || position < 0.0f)
        {
            if (position > 1.0f)
            {
                position = 2.0f - position;
                ping_pong_direction_ = -1;
            }
            else
            {
                position = -position;
                ping_pong_direction_ = 1;
            }
        }
        position = std::clamp(position, 0.0f, 1.0f);
        break;
    }
}

SplitMotionController &split_motion() noexcept
{
    return g_split_motion;
}

bool should_capture_pair(bool frozen, bool pair_ready) noexcept
{
    return !frozen || !pair_ready;
}
}
