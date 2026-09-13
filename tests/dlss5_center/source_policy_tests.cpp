#include "integrations/dlss5/center/source_policy.hpp"

#include <cstdlib>
#include <iostream>

namespace
{
using namespace framecompare::dlss5::center;

void require(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void generic_modes_keep_the_generic_before()
{
    require(select_source({false, true, true, 4, 3}) ==
                BeforeSource::generic,
            "disabled DLSS5 should keep the generic source");
    require(select_source({true, false, true, 4, 3}) ==
                BeforeSource::generic,
            "same-coordinate mode should keep its in-place path");
}

void center_mode_requires_a_fresh_complete_capture()
{
    require(select_source({true, true, false, 4, 3}) ==
                BeforeSource::generic,
            "an unavailable bridge should fail open");
    require(select_source({true, true, true, 3, 3}) ==
                BeforeSource::generic,
            "a consumed generation should not be reused");
    require(select_source({true, true, true, 4, 3}) ==
                BeforeSource::dlss_input,
            "a fresh center capture should become the Before source");
}

void first_pass_is_latched_until_the_frame_is_consumed()
{
    FirstPassLatch latch;
    require(latch.try_reserve(), "the first pass should reserve capture");
    require(!latch.try_reserve(),
            "later passes should preserve the first captured input");
    require(latch.pending(), "the first pass should remain pending");
    latch.release();
    require(!latch.pending(), "consumption should release the latch");
    require(latch.try_reserve(), "the next frame should capture again");
}
}

int main()
{
    generic_modes_keep_the_generic_before();
    center_mode_requires_a_fresh_complete_capture();
    first_pass_is_latched_until_the_frame_is_consumed();
    std::cout << "DLSS5 center source policy tests passed.\n";
    return 0;
}
