#include "core/frame_pair.hpp"

namespace framecompare
{
bool FrameToken::valid() const noexcept
{
    return runtime_id != 0 && frame_sequence != 0;
}

bool operator==(FrameToken left, FrameToken right) noexcept
{
    return left.runtime_id == right.runtime_id &&
        left.frame_sequence == right.frame_sequence;
}

bool is_verified_vanilla(CaptureProvenance provenance) noexcept
{
    return provenance == CaptureProvenance::verified_vanilla;
}

SubmitResult FramePairCoordinator::submit_before(CaptureStamp stamp) noexcept
{
    reset();
    if (!valid(stamp))
        return SubmitResult::invalid_stamp;

    before_ = stamp;
    has_before_ = true;
    return SubmitResult::accepted;
}

SubmitResult FramePairCoordinator::submit_after(CaptureStamp stamp) noexcept
{
    has_after_ = false;
    if (!valid(stamp))
        return SubmitResult::invalid_stamp;
    if (!has_before_)
        return SubmitResult::missing_before;
    if (!same_token(before_, stamp))
        return SubmitResult::token_mismatch;

    after_ = stamp;
    has_after_ = true;
    return SubmitResult::accepted;
}

bool FramePairCoordinator::ready() const noexcept
{
    return has_before_ && has_after_ && same_token(before_, after_);
}

const CaptureStamp &FramePairCoordinator::before() const noexcept
{
    return before_;
}

const CaptureStamp &FramePairCoordinator::after() const noexcept
{
    return after_;
}

void FramePairCoordinator::reset() noexcept
{
    before_ = {};
    after_ = {};
    has_before_ = false;
    has_after_ = false;
}

bool FramePairCoordinator::valid(CaptureStamp stamp) noexcept
{
    return stamp.runtime_id != 0 && stamp.frame_sequence != 0 &&
        stamp.provenance != CaptureProvenance::unknown;
}

bool FramePairCoordinator::same_token(CaptureStamp left, CaptureStamp right) noexcept
{
    return left.runtime_id == right.runtime_id &&
        left.frame_sequence == right.frame_sequence;
}
}
