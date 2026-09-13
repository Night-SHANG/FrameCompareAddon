#include "integrations/dlss5/center/source_policy.hpp"

namespace framecompare::dlss5::center
{
BeforeSource select_source(SourceConditions conditions) noexcept
{
    return conditions.dlss5_enabled && conditions.center_remap &&
            conditions.bridge_ready && conditions.available_generation != 0 &&
            conditions.available_generation > conditions.consumed_generation
        ? BeforeSource::dlss_input
        : BeforeSource::generic;
}
}

