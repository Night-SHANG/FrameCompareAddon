#pragma once

// Minimal NGX ABI declarations used only to observe existing calls made by the host.
// No NVIDIA SDK header or implementation is redistributed by this project.

#include <Windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <cstdint>

#if defined(_MSC_VER)
#define FRAMECOMPARE_NGX_CALL __cdecl
#else
#define FRAMECOMPARE_NGX_CALL
#endif

using NVSDK_NGX_Result = std::uint32_t;
using NVSDK_NGX_Feature = std::int32_t;

struct NVSDK_NGX_Handle;

struct NVSDK_NGX_Parameter
{
    virtual void Set(const char *, unsigned long long) = 0;
    virtual void Set(const char *, float) = 0;
    virtual void Set(const char *, double) = 0;
    virtual void Set(const char *, unsigned int) = 0;
    virtual void Set(const char *, int) = 0;
    virtual void Set(const char *, ID3D11Resource *) = 0;
    virtual void Set(const char *, ID3D12Resource *) = 0;
    virtual void Set(const char *, void *) = 0;

    virtual NVSDK_NGX_Result Get(const char *, unsigned long long *) const = 0;
    virtual NVSDK_NGX_Result Get(const char *, float *) const = 0;
    virtual NVSDK_NGX_Result Get(const char *, double *) const = 0;
    virtual NVSDK_NGX_Result Get(const char *, unsigned int *) const = 0;
    virtual NVSDK_NGX_Result Get(const char *, int *) const = 0;
    virtual NVSDK_NGX_Result Get(const char *, ID3D11Resource **) const = 0;
    virtual NVSDK_NGX_Result Get(const char *, ID3D12Resource **) const = 0;
    virtual NVSDK_NGX_Result Get(const char *, void **) const = 0;

    virtual void Reset() = 0;
};

using PFN_NVSDK_NGX_ProgressCallback = void (FRAMECOMPARE_NGX_CALL *)(float, bool &);
using PFN_NVSDK_NGX_ProgressCallback_C = void (FRAMECOMPARE_NGX_CALL *)(float, bool *);

constexpr NVSDK_NGX_Feature FRAMECOMPARE_NGX_TARGET_FEATURE = 18;
constexpr char FRAMECOMPARE_NGX_PARAMETER_COLOR[] = "Color";

inline bool framecompare_ngx_succeeded(NVSDK_NGX_Result value) noexcept
{
    return (value & 0xFFF00000u) != 0xBAD00000u;
}

inline bool framecompare_ngx_failed(NVSDK_NGX_Result value) noexcept
{
    return !framecompare_ngx_succeeded(value);
}
