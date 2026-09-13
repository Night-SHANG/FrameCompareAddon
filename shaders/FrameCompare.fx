#include "ReShade.fxh"

texture FrameCompareBefore : FRAMECOMPARE_BEFORE;
texture FrameCompareAfter : FRAMECOMPARE_AFTER;
texture FrameCompareParams : FRAMECOMPARE_PARAMS;

sampler BeforeSampler { Texture = FrameCompareBefore; AddressU = CLAMP; AddressV = CLAMP; };
sampler AfterSampler { Texture = FrameCompareAfter; AddressU = CLAMP; AddressV = CLAMP; };
sampler ParamsSampler { Texture = FrameCompareParams; MinFilter = POINT; MagFilter = POINT; };

float4 ReadParams(int index)
{
    return tex2D(ParamsSampler, float2((index + 0.5) / 2.0, 0.5));
}

float CenterSource(float split, float center_focus)
{
    split = saturate(split);
    const float widest_region = max(split, 1.0 - split);
    const float minimum_center = widest_region * 0.5;
    return lerp(minimum_center, 1.0 - minimum_center,
                saturate(center_focus));
}

float2 BeforeUV(float2 uv, float split, bool before_left, int mode,
                float center_focus)
{
    if (mode == 0)
        return uv;
    uv.x += CenterSource(split, center_focus) - 0.5;
    uv.x = before_left
        ? saturate(uv.x + 0.5 * (1.0 - split))
        : saturate(uv.x - 0.5 * split);
    return uv;
}

float2 AfterUV(float2 uv, float split, bool before_left, int mode,
               float center_focus)
{
    if (mode == 0)
        return uv;
    uv.x += CenterSource(split, center_focus) - 0.5;
    uv.x = before_left
        ? saturate(uv.x - 0.5 * split)
        : saturate(uv.x + 0.5 * (1.0 - split));
    return uv;
}

float4 PS_FrameCompare(float4 position : SV_Position,
                       float2 uv : TEXCOORD) : SV_Target
{
    const float4 p0 = ReadParams(0);
    const float4 p1 = ReadParams(1);
    const float split = p0.x;
    const bool before_left = p1.x >= 0.5;
    const int mode = int(p1.y + 0.5);
    const float center_focus = p1.w;

    if (p1.z < 0.5)
        return tex2D(ReShade::BackBuffer, uv);

    const bool before_region = before_left ? uv.x < split : uv.x >= split;
    float4 color = before_region
        ? tex2D(BeforeSampler,
                BeforeUV(uv, split, before_left, mode, center_focus))
        : tex2D(AfterSampler,
                AfterUV(uv, split, before_left, mode, center_focus));

    const bool interior = split > 0.0 && split < 1.0;
    if (p0.w >= 0.5 && interior && abs(uv.x - split) <= p0.y * 0.5)
        color = lerp(color, float4(0.95, 0.95, 0.90, 1.0), p0.z);
    return color;
}

technique FrameCompareComposite < enabled = false; >
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader = PS_FrameCompare;
    }
}
