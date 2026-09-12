#pragma once

#include "core/frame_pair.hpp"

namespace framecompare
{
class CaptureCycleTracker
{
public:
    explicit CaptureCycleTracker(std::uintptr_t runtime_id) noexcept;

    FrameToken begin() noexcept;
    FrameToken finish() noexcept;
    void reset() noexcept;

private:
    std::uintptr_t runtime_id_ = 0;
    std::uint64_t next_sequence_ = 1;
    FrameToken active_{};
};
}
