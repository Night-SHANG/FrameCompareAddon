#include "core/capture_cycle.hpp"
#include "core/frame_pair.hpp"

#include <cstdlib>
#include <iostream>

namespace
{
using framecompare::CaptureProvenance;
using framecompare::CaptureStamp;
using framecompare::CaptureCycleTracker;
using framecompare::FramePairCoordinator;
using framecompare::SubmitResult;

void require(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void matching_stamps_form_a_ready_pair()
{
    FramePairCoordinator coordinator;
    const CaptureStamp before{17, 42, CaptureProvenance::pre_reshade_fx};
    const CaptureStamp after{17, 42, CaptureProvenance::post_reshade_fx};

    require(coordinator.submit_before(before) == SubmitResult::accepted,
            "matching Before should be accepted");
    require(coordinator.submit_after(after) == SubmitResult::accepted,
            "matching After should be accepted");
    require(coordinator.ready(), "matching stamps should form a ready pair");
}

void a_different_sequence_cannot_reuse_an_old_before()
{
    FramePairCoordinator coordinator;
    coordinator.submit_before({17, 42, CaptureProvenance::pre_reshade_fx});

    require(coordinator.submit_after({17, 43, CaptureProvenance::post_reshade_fx}) ==
                SubmitResult::token_mismatch,
            "different frame sequence should be rejected");
    require(!coordinator.ready(), "mismatched frames must not be composited");
}

void a_different_runtime_cannot_form_a_pair()
{
    FramePairCoordinator coordinator;
    coordinator.submit_before({17, 42, CaptureProvenance::pre_reshade_fx});

    require(coordinator.submit_after({18, 42, CaptureProvenance::post_reshade_fx}) ==
                SubmitResult::token_mismatch,
            "different runtime should be rejected");
    require(!coordinator.ready(), "different runtimes must not be composited");
}

void pre_reshade_is_not_verified_vanilla()
{
    require(!framecompare::is_verified_vanilla(CaptureProvenance::pre_reshade_fx),
            "Pre-ReShade FX must not be presented as verified Vanilla");
    require(framecompare::is_verified_vanilla(CaptureProvenance::verified_vanilla),
            "only the verified provenance is Vanilla");
}

void one_effect_cycle_uses_one_stable_token()
{
    CaptureCycleTracker tracker(17);
    const auto before = tracker.begin();

    require(before.runtime_id == 17 && before.frame_sequence == 1,
            "the first cycle should produce token 1");
    require(tracker.finish() == before,
            "finish should return the token created by begin");
    require(!tracker.finish().valid(),
            "a duplicate finish must not reuse a completed token");

    const auto next = tracker.begin();
    require(next.frame_sequence == 2,
            "the next begin should advance exactly once");
}
}

int main()
{
    matching_stamps_form_a_ready_pair();
    a_different_sequence_cannot_reuse_an_old_before();
    a_different_runtime_cannot_form_a_pair();
    pre_reshade_is_not_verified_vanilla();
    one_effect_cycle_uses_one_stable_token();
    std::cout << "frame_pair_tests: PASS\n";
}
