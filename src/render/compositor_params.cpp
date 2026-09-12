#include "render/compositor_params.hpp"

#include <algorithm>

namespace framecompare::render
{
ShaderParams make_shader_params(const CompositorSettings &settings,
                                bool pair_ready) noexcept
{
    ShaderParams result;
    result.split_position = std::clamp(settings.split_position, 0.0f, 1.0f);
    result.border_width = std::max(settings.border_width, 0.0f);
    result.border_opacity = std::clamp(settings.border_opacity, 0.0f, 1.0f);
    result.show_border = settings.show_border ? 1.0f : 0.0f;
    result.before_on_left = settings.before_on_left ? 1.0f : 0.0f;
    result.display_mode = static_cast<float>(settings.display_mode);
    result.pair_ready = settings.enabled && pair_ready ? 1.0f : 0.0f;
    return result;
}
}
