// FrameCompare companion compositor for the FrameCompare ReShade add-on.
// v1.3: the add-on binds Before/After plus a tiny parameter texture and renders
// this technique explicitly. Labels/HUD are drawn by the add-on's ImGui OSD,
// so this shader contains only the comparison image itself.

#include "ReShade.fxh"

texture FrameCompareBefore : FRAMECOMPARE_BEFORE;
texture FrameCompareAfter : FRAMECOMPARE_AFTER;
texture FrameCompareParams : FRAMECOMPARE_PARAMS;

sampler BeforeSampler
{
    Texture = FrameCompareBefore;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = POINT;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

sampler AfterSampler
{
    Texture = FrameCompareAfter;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = POINT;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

sampler ParamsSampler
{
    Texture = FrameCompareParams;
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = POINT;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

float4 ReadParam(int index)
{
    return tex2D(ParamsSampler, float2((index + 0.5) / 8.0, 0.5));
}

bool IsBeforeRegion(float x, float split, bool before_on_left)
{
    return before_on_left ? (x < split) : (x >= split);
}

// SplitScreenCR uses a characteristic horizontal center remap rather than a
// plain same-coordinate wipe. At a 50/50 split the left half is shifted by
// +0.25 UV and the right half by -0.25 UV. This independently reimplements
// that visible behavior while making the 0%/100% endpoints become true full
// frames, so automated sweeps can cleanly start/end on Before/After.
float2 SplitScreenCRBeforeUV(float2 uv, float split, bool before_on_left)
{
    if (before_on_left)
        uv.x = saturate(uv.x + 0.5 * (1.0 - split));
    else
        uv.x = saturate(uv.x - 0.5 * split);
    return uv;
}

float2 SplitScreenCRAfterUV(float2 uv, float split, bool before_on_left)
{
    if (before_on_left)
        uv.x = saturate(uv.x - 0.5 * split);
    else
        uv.x = saturate(uv.x + 0.5 * (1.0 - split));
    return uv;
}

float4 PS_FrameCompare(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    // p0 = split, border width, border opacity, show border
    // p1 = before-on-left, display mode, pair-valid, reserved
    float4 p0 = ReadParam(0);
    float4 p1 = ReadParam(1);

    float split = saturate(p0.x);
    bool before_on_left = p1.x >= 0.5;
    bool split_screen_cr = p1.y >= 0.5;
    bool pair_valid = p1.z >= 0.5;

    if (!pair_valid)
        return tex2D(ReShade::BackBuffer, uv);

    bool before_region = IsBeforeRegion(uv.x, split, before_on_left);
    float2 before_uv = split_screen_cr ? SplitScreenCRBeforeUV(uv, split, before_on_left) : uv;
    float2 after_uv = split_screen_cr ? SplitScreenCRAfterUV(uv, split, before_on_left) : uv;

    float4 color = before_region ? tex2D(BeforeSampler, before_uv) : tex2D(AfterSampler, after_uv);

    if (p0.w >= 0.5 && p0.y > 0.0 && abs(uv.x - split) <= p0.y)
    {
        float border_alpha = saturate(p0.z);
        color.rgb = lerp(color.rgb, float3(0.95, 0.95, 0.90), border_alpha);
    }

    return color;
}

technique FrameCompareComposite
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader = PS_FrameCompare;
    }
}
