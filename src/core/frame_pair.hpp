#pragma once

#include <cstdint>

namespace framecompare
{
struct FrameToken
{
    std::uintptr_t runtime_id = 0;
    std::uint64_t frame_sequence = 0;

    bool valid() const noexcept;
};

bool operator==(FrameToken left, FrameToken right) noexcept;

enum class CaptureProvenance : std::uint8_t
{
    unknown,
    pre_reshade_fx,
    post_reshade_fx,
    dlss_nr_input,
    verified_vanilla,
};

struct CaptureStamp
{
    std::uintptr_t runtime_id = 0;
    std::uint64_t frame_sequence = 0;
    CaptureProvenance provenance = CaptureProvenance::unknown;
};

enum class SubmitResult : std::uint8_t
{
    accepted,
    invalid_stamp,
    missing_before,
    token_mismatch,
};

bool is_verified_vanilla(CaptureProvenance provenance) noexcept;

class FramePairCoordinator
{
public:
    SubmitResult submit_before(CaptureStamp stamp) noexcept;
    SubmitResult submit_after(CaptureStamp stamp) noexcept;
    bool ready() const noexcept;
    const CaptureStamp &before() const noexcept;
    const CaptureStamp &after() const noexcept;
    void reset() noexcept;

private:
    static bool valid(CaptureStamp stamp) noexcept;
    static bool same_token(CaptureStamp left, CaptureStamp right) noexcept;

    CaptureStamp before_{};
    CaptureStamp after_{};
    bool has_before_ = false;
    bool has_after_ = false;
};
}
