#include "integrations/dlss5/gpu/d3d11_copy.hpp"

namespace framecompare::dlss5
{
namespace
{
TextureShape shape(const D3D11_TEXTURE2D_DESC &desc) noexcept
{
    return {desc.Width, desc.Height, static_cast<std::uint32_t>(desc.Format),
            desc.MipLevels, desc.ArraySize, desc.SampleDesc.Count,
            desc.SampleDesc.Quality};
}

CopyOutcome apply_impl(ID3D11DeviceContext *context, ID3D11Resource *color,
                       ID3D11Resource *output,
                       const CopyRegion &region) noexcept
{
    if (context == nullptr || color == nullptr || output == nullptr)
        return CopyOutcome::incomplete_parameters;
    if (color == output)
        return CopyOutcome::incompatible;
    if (region.empty())
        return CopyOutcome::empty_region;

    ID3D11Texture2D *source = nullptr;
    ID3D11Texture2D *destination = nullptr;
    if (FAILED(color->QueryInterface(IID_PPV_ARGS(&source))) ||
        source == nullptr)
        return CopyOutcome::incompatible;
    if (FAILED(output->QueryInterface(IID_PPV_ARGS(&destination))) ||
        destination == nullptr)
    {
        source->Release();
        return CopyOutcome::incompatible;
    }

    D3D11_TEXTURE2D_DESC source_desc{};
    D3D11_TEXTURE2D_DESC destination_desc{};
    source->GetDesc(&source_desc);
    destination->GetDesc(&destination_desc);

    ID3D11Device *context_device = nullptr;
    ID3D11Device *source_device = nullptr;
    ID3D11Device *destination_device = nullptr;
    context->GetDevice(&context_device);
    source->GetDevice(&source_device);
    destination->GetDevice(&destination_device);
    const bool same_device = context_device != nullptr &&
        context_device == source_device && source_device == destination_device;
    if (context_device != nullptr) context_device->Release();
    if (source_device != nullptr) source_device->Release();
    if (destination_device != nullptr) destination_device->Release();

    const Compatibility compatibility = validate_copy(
        shape(source_desc), shape(destination_desc));
    if (!same_device || compatibility != Compatibility::compatible ||
        region.right > destination_desc.Width ||
        region.bottom > destination_desc.Height)
    {
        source->Release();
        destination->Release();
        return !same_device ? CopyOutcome::device_mismatch
                            : CopyOutcome::incompatible;
    }

    const D3D11_BOX box{region.left, region.top, 0, region.right,
                        region.bottom, 1};
    context->CopySubresourceRegion(output, 0, region.left, region.top, 0,
                                   color, 0, &box);
    source->Release();
    destination->Release();
    return CopyOutcome::applied;
}
}

CopyOutcome apply_d3d11_copy(ID3D11DeviceContext *context,
                             ID3D11Resource *color,
                             ID3D11Resource *output,
                             const CopyRegion &region) noexcept
{
#if defined(_MSC_VER)
    __try { return apply_impl(context, color, output, region); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return CopyOutcome::guarded_failure; }
#else
    return apply_impl(context, color, output, region);
#endif
}
}
