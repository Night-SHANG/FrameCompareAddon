#include "integrations/dlss5/gpu/d3d12_copy.hpp"

#include <limits>

namespace framecompare::dlss5
{
namespace
{
TextureShape shape(const D3D12_RESOURCE_DESC &desc) noexcept
{
    return {static_cast<std::uint32_t>(desc.Width), desc.Height,
            static_cast<std::uint32_t>(desc.Format), desc.MipLevels,
            desc.DepthOrArraySize, desc.SampleDesc.Count,
            desc.SampleDesc.Quality};
}

D3D12_RESOURCE_BARRIER transition(ID3D12Resource *resource,
                                  D3D12_RESOURCE_STATES before,
                                  D3D12_RESOURCE_STATES after) noexcept
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    return barrier;
}

CopyOutcome apply_impl(ID3D12GraphicsCommandList *commands,
                       ID3D12Resource *color, ID3D12Resource *output,
                       const CopyRegion &region) noexcept
{
    if (commands == nullptr || color == nullptr || output == nullptr)
        return CopyOutcome::incomplete_parameters;
    if (color == output)
        return CopyOutcome::incompatible;
    if (region.empty())
        return CopyOutcome::empty_region;

    const D3D12_RESOURCE_DESC source_desc = color->GetDesc();
    const D3D12_RESOURCE_DESC destination_desc = output->GetDesc();
    if (source_desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
        destination_desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
        source_desc.Width > std::numeric_limits<std::uint32_t>::max() ||
        destination_desc.Width > std::numeric_limits<std::uint32_t>::max() ||
        validate_copy(shape(source_desc), shape(destination_desc)) !=
            Compatibility::compatible ||
        region.right > destination_desc.Width ||
        region.bottom > destination_desc.Height)
        return CopyOutcome::incompatible;

    ID3D12Device *command_device = nullptr;
    ID3D12Device *source_device = nullptr;
    ID3D12Device *destination_device = nullptr;
    const bool same_device =
        SUCCEEDED(commands->GetDevice(IID_PPV_ARGS(&command_device))) &&
        SUCCEEDED(color->GetDevice(IID_PPV_ARGS(&source_device))) &&
        SUCCEEDED(output->GetDevice(IID_PPV_ARGS(&destination_device))) &&
        command_device != nullptr && command_device == source_device &&
        source_device == destination_device;
    if (command_device != nullptr) command_device->Release();
    if (source_device != nullptr) source_device->Release();
    if (destination_device != nullptr) destination_device->Release();
    if (!same_device)
        return CopyOutcome::device_mismatch;

    D3D12_RESOURCE_BARRIER uav{};
    uav.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uav.UAV.pResource = output;
    commands->ResourceBarrier(1, &uav);

    D3D12_RESOURCE_BARRIER to_copy[2] = {
        transition(color, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                   D3D12_RESOURCE_STATE_COPY_SOURCE),
        transition(output, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                   D3D12_RESOURCE_STATE_COPY_DEST),
    };
    commands->ResourceBarrier(2, to_copy);

    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource = color;
    source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION destination{};
    destination.pResource = output;
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    const D3D12_BOX box{region.left, region.top, 0, region.right,
                        region.bottom, 1};
    commands->CopyTextureRegion(&destination, region.left, region.top, 0,
                                &source, &box);

    D3D12_RESOURCE_BARRIER restore[2] = {
        transition(color, D3D12_RESOURCE_STATE_COPY_SOURCE,
                   D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
        transition(output, D3D12_RESOURCE_STATE_COPY_DEST,
                   D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
    };
    commands->ResourceBarrier(2, restore);
    return CopyOutcome::applied;
}
}

CopyOutcome apply_d3d12_copy(ID3D12GraphicsCommandList *commands,
                             ID3D12Resource *color,
                             ID3D12Resource *output,
                             const CopyRegion &region) noexcept
{
#if defined(_MSC_VER)
    __try { return apply_impl(commands, color, output, region); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return CopyOutcome::guarded_failure; }
#else
    return apply_impl(commands, color, output, region);
#endif
}
}
