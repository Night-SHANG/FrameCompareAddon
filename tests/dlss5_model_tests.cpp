#include "integrations/dlss5/model.hpp"
#include "integrations/dlss5/runtime.hpp"

#include <cassert>
#include <cstdint>

using namespace framecompare::dlss5;

namespace
{
TextureShape texture(std::uint32_t width = 1920,
                     std::uint32_t height = 1080,
                     std::uint32_t format = 28)
{
    return {width, height, format, 1, 1, 1, 0};
}
}

int main()
{
    assert(validate_copy(texture(), texture()) ==
           Compatibility::compatible);

    auto mismatched = texture();
    mismatched.width = 1280;
    assert(validate_copy(texture(), mismatched) ==
           Compatibility::dimensions_mismatch);

    auto multisampled = texture();
    multisampled.sample_count = 2;
    assert(validate_copy(multisampled, multisampled) ==
           Compatibility::multisampled);

    const CopyRegion left = make_copy_region(1920, 1080, 0.25f, true);
    assert(left.left == 0 && left.right == 480);
    assert(left.top == 0 && left.bottom == 1080);
    assert(!left.empty());

    const CopyRegion right = make_copy_region(1920, 1080, 0.25f, false);
    assert(right.left == 480 && right.right == 1920);
    assert(!right.empty());

    assert(make_copy_region(1920, 1080, 0.0f, true).empty());
    assert(make_copy_region(1920, 1080, 1.0f, false).empty());

    publish_settings({Operation::center_full_frame, 0.375f, false});
    const SettingsSnapshot settings = settings_snapshot();
    assert(settings.operation == Operation::center_full_frame);
    assert(settings.split_position == 0.375f);
    assert(!settings.before_on_left);

    publish_settings({Operation::same_coordinate_region, 0.625f, true});
    const SettingsSnapshot same_coordinate = settings_snapshot();
    assert(same_coordinate.operation == Operation::same_coordinate_region);
    assert(same_coordinate.split_position == 0.625f);
    assert(same_coordinate.before_on_left);

    record_result(Api::d3d12, CopyOutcome::applied,
                  make_copy_region(1920, 1080, 0.5f, true));
    record_result(Api::d3d11, CopyOutcome::disabled, {});
    record_result(Api::d3d11, CopyOutcome::retained_first_pass, {});
    const DiagnosticsSnapshot diagnostics = diagnostics_snapshot();
    assert(diagnostics.d3d12_applied == 1);
    assert(diagnostics.d3d11_skipped == 1);
    assert(diagnostics.last_api == Api::d3d11);
    assert(diagnostics.last_outcome == CopyOutcome::retained_first_pass);
    assert(copy_outcome_name(CopyOutcome::incompatible) ==
           std::string_view("incompatible"));
    assert(copy_outcome_name(CopyOutcome::retained_first_pass) ==
           std::string_view("first-pass-retained"));
}
