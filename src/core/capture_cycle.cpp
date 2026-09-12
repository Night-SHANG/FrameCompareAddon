#include "core/capture_cycle.hpp"

namespace framecompare
{
CaptureCycleTracker::CaptureCycleTracker(std::uintptr_t runtime_id) noexcept
    : runtime_id_(runtime_id)
{
}

FrameToken CaptureCycleTracker::begin() noexcept
{
    if (active_.valid() || runtime_id_ == 0)
        return {};

    active_ = {runtime_id_, next_sequence_++};
    return active_;
}

FrameToken CaptureCycleTracker::finish() noexcept
{
    const FrameToken completed = active_;
    active_ = {};
    return completed;
}

void CaptureCycleTracker::reset() noexcept
{
    active_ = {};
}
}
