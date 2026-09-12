#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <d3d12.h>

#include <cstdint>

#if defined(_MSC_VER)
#define FRAMECOMPARE_NGX_CALL __cdecl
#else
#define FRAMECOMPARE_NGX_CALL
#endif

namespace framecompare::dlss5::abi
{
using Result = std::uint32_t;
struct Handle;

struct Parameter
{
    virtual void Set(const char *, unsigned long long) = 0;
    virtual void Set(const char *, float) = 0;
    virtual void Set(const char *, double) = 0;
    virtual void Set(const char *, unsigned int) = 0;
    virtual void Set(const char *, int) = 0;
    virtual void Set(const char *, ID3D11Resource *) = 0;
    virtual void Set(const char *, ID3D12Resource *) = 0;
    virtual void Set(const char *, void *) = 0;
    virtual Result Get(const char *, unsigned long long *) const = 0;
    virtual Result Get(const char *, float *) const = 0;
    virtual Result Get(const char *, double *) const = 0;
    virtual Result Get(const char *, unsigned int *) const = 0;
    virtual Result Get(const char *, int *) const = 0;
    virtual Result Get(const char *, ID3D11Resource **) const = 0;
    virtual Result Get(const char *, ID3D12Resource **) const = 0;
    virtual Result Get(const char *, void **) const = 0;
    virtual void Reset() = 0;
};

using ProgressCallback = void (FRAMECOMPARE_NGX_CALL *)(float, bool &);
using ProgressCallbackC = void (FRAMECOMPARE_NGX_CALL *)(float, bool *);
using Evaluate11 = Result (FRAMECOMPARE_NGX_CALL *)(
    ID3D11DeviceContext *, const Handle *, const Parameter *, ProgressCallback);
using Evaluate11C = Result (FRAMECOMPARE_NGX_CALL *)(
    ID3D11DeviceContext *, const Handle *, const Parameter *, ProgressCallbackC);
using Evaluate12 = Result (FRAMECOMPARE_NGX_CALL *)(
    ID3D12GraphicsCommandList *, const Handle *, const Parameter *, ProgressCallback);
using Evaluate12C = Result (FRAMECOMPARE_NGX_CALL *)(
    ID3D12GraphicsCommandList *, const Handle *, const Parameter *, ProgressCallbackC);

inline bool succeeded(Result value) noexcept
{
    return (value & 0xFFF00000u) != 0xBAD00000u;
}
}
