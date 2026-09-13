#include "integrations/dlss5/center/source_policy.hpp"

namespace framecompare::dlss5::center
{
bool FirstPassLatch::try_reserve() noexcept
{
    if (pending_) return false;
    pending_ = true;
    return true;
}

bool FirstPassLatch::pending() const noexcept { return pending_; }

void FirstPassLatch::release() noexcept { pending_ = false; }

BeforeSource select_source(SourceConditions conditions) noexcept
{
    return conditions.dlss5_enabled && conditions.center_remap &&
            conditions.bridge_ready && conditions.available_generation != 0 &&
            conditions.available_generation > conditions.consumed_generation
        ? BeforeSource::dlss_input
        : BeforeSource::generic;
}
}
