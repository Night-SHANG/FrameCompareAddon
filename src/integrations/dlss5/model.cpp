#include "integrations/dlss5/model.hpp"

#include <algorithm>
#include <cmath>

namespace framecompare::dlss5
{
bool CopyRegion::empty() const noexcept
{
    return right <= left || bottom <= top;
}

Compatibility validate_copy(const TextureShape &source,
                            const TextureShape &destination) noexcept
{
    if (source.width == 0 || source.height == 0 ||
        destination.width == 0 || destination.height == 0)
        return Compatibility::invalid_dimensions;
    if (source.width != destination.width ||
        source.height != destination.height)
        return Compatibility::dimensions_mismatch;
    if (source.format != destination.format)
        return Compatibility::format_mismatch;
    if (source.mip_levels != destination.mip_levels ||
        source.array_size != destination.array_size)
        return Compatibility::subresource_mismatch;
    if (source.sample_count != 1 || destination.sample_count != 1 ||
        source.sample_quality != destination.sample_quality)
        return Compatibility::multisampled;
    return Compatibility::compatible;
}

CopyRegion make_copy_region(std::uint32_t width, std::uint32_t height,
                            float split_position,
                            bool before_on_left) noexcept
{
    const float finite = std::isfinite(split_position) ? split_position : 0.5f;
    const float clamped = std::clamp(finite, 0.0f, 1.0f);
    const auto split = static_cast<std::uint32_t>(
        std::lround(clamped * static_cast<float>(width)));
    return before_on_left ? CopyRegion{0, 0, split, height}
                          : CopyRegion{split, 0, width, height};
}

std::string_view copy_outcome_name(CopyOutcome outcome) noexcept
{
    switch (outcome)
    {
    case CopyOutcome::applied: return "applied";
    case CopyOutcome::disabled: return "disabled";
    case CopyOutcome::incomplete_parameters: return "incomplete-parameters";
    case CopyOutcome::incompatible: return "incompatible";
    case CopyOutcome::empty_region: return "empty-region";
    case CopyOutcome::device_mismatch: return "device-mismatch";
    case CopyOutcome::guarded_failure: return "guarded-failure";
    case CopyOutcome::none:
    default: return "none";
    }
}
}
