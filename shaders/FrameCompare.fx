// FrameCompare companion compositor for the FrameCompare ReShade add-on.
// The add-on binds all semantic textures and renders this technique explicitly.

#include "ReShade.fxh"

texture FrameCompareBefore : FRAMECOMPARE_BEFORE;
texture FrameCompareAfter : FRAMECOMPARE_AFTER;
texture FrameCompareParams : FRAMECOMPARE_PARAMS;
texture FrameCompareLabelBefore : FRAMECOMPARE_LABEL_BEFORE;
texture FrameCompareLabelAfter : FRAMECOMPARE_LABEL_AFTER;

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

sampler BeforeLabelSampler
{
    Texture = FrameCompareLabelBefore;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = POINT;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

sampler AfterLabelSampler
{
    Texture = FrameCompareLabelAfter;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
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

float2 LegacyBeforeUV(float2 uv, float split, bool before_on_left)
{
    // At split=0.5 this reproduces the characteristic +/-0.25 center-offset
    // sampling of the old SplitScreenCR display, while still reaching an
    // unmodified full frame at the 0%/100% endpoints.
    if (before_on_left)
        uv.x = saturate(uv.x + 0.5 * (1.0 - split));
    else
        uv.x = saturate(uv.x - 0.5 * split);
    return uv;
}

float2 LegacyAfterUV(float2 uv, float split, bool before_on_left)
{
    if (before_on_left)
        uv.x = saturate(uv.x - 0.5 * split);
    else
        uv.x = saturate(uv.x + 0.5 * (1.0 - split));
    return uv;
}

float SampleOutlineBefore(float2 label_uv, float2 texel, float radius)
{
    float2 d = texel * max(radius, 0.0);
    float a = 0.0;
    a = max(a, tex2D(BeforeLabelSampler, label_uv + float2( d.x, 0.0)).a);
    a = max(a, tex2D(BeforeLabelSampler, label_uv + float2(-d.x, 0.0)).a);
    a = max(a, tex2D(BeforeLabelSampler, label_uv + float2(0.0,  d.y)).a);
    a = max(a, tex2D(BeforeLabelSampler, label_uv + float2(0.0, -d.y)).a);
    a = max(a, tex2D(BeforeLabelSampler, label_uv + float2( d.x,  d.y)).a);
    a = max(a, tex2D(BeforeLabelSampler, label_uv + float2(-d.x,  d.y)).a);
    a = max(a, tex2D(BeforeLabelSampler, label_uv + float2( d.x, -d.y)).a);
    a = max(a, tex2D(BeforeLabelSampler, label_uv + float2(-d.x, -d.y)).a);
    return a;
}

float SampleOutlineAfter(float2 label_uv, float2 texel, float radius)
{
    float2 d = texel * max(radius, 0.0);
    float a = 0.0;
    a = max(a, tex2D(AfterLabelSampler, label_uv + float2( d.x, 0.0)).a);
    a = max(a, tex2D(AfterLabelSampler, label_uv + float2(-d.x, 0.0)).a);
    a = max(a, tex2D(AfterLabelSampler, label_uv + float2(0.0,  d.y)).a);
    a = max(a, tex2D(AfterLabelSampler, label_uv + float2(0.0, -d.y)).a);
    a = max(a, tex2D(AfterLabelSampler, label_uv + float2( d.x,  d.y)).a);
    a = max(a, tex2D(AfterLabelSampler, label_uv + float2(-d.x,  d.y)).a);
    a = max(a, tex2D(AfterLabelSampler, label_uv + float2( d.x, -d.y)).a);
    a = max(a, tex2D(AfterLabelSampler, label_uv + float2(-d.x, -d.y)).a);
    return a;
}

float4 BlendLabelBefore(float4 base, float2 uv, float4 rect, float2 texel, float opacity, float outline_px)
{
    if (rect.z <= 0.0 || rect.w <= 0.0 || uv.x < rect.x || uv.y < rect.y || uv.x >= rect.x + rect.z || uv.y >= rect.y + rect.w)
        return base;

    float2 luv = (uv - rect.xy) / rect.zw;
    float glyph = tex2D(BeforeLabelSampler, luv).a * opacity;
    float outline = SampleOutlineBefore(luv, texel, outline_px) * opacity;
    float outline_only = saturate(outline - glyph);
    base.rgb = lerp(base.rgb, float3(0.0, 0.0, 0.0), outline_only);
    base.rgb = lerp(base.rgb, float3(1.0, 1.0, 1.0), glyph);
    return base;
}

float4 BlendLabelAfter(float4 base, float2 uv, float4 rect, float2 texel, float opacity, float outline_px)
{
    if (rect.z <= 0.0 || rect.w <= 0.0 || uv.x < rect.x || uv.y < rect.y || uv.x >= rect.x + rect.z || uv.y >= rect.y + rect.w)
        return base;

    float2 luv = (uv - rect.xy) / rect.zw;
    float glyph = tex2D(AfterLabelSampler, luv).a * opacity;
    float outline = SampleOutlineAfter(luv, texel, outline_px) * opacity;
    float outline_only = saturate(outline - glyph);
    base.rgb = lerp(base.rgb, float3(0.0, 0.0, 0.0), outline_only);
    base.rgb = lerp(base.rgb, float3(1.0, 1.0, 1.0), glyph);
    return base;
}

float4 PS_FrameCompare(float4 pos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float4 p0 = ReadParam(0); // split, border width, border opacity, show border
    float4 p1 = ReadParam(1); // before-left, labels, label opacity, outline px
    float4 p2 = ReadParam(2); // before label rect
    float4 p3 = ReadParam(3); // after label rect
    float4 p4 = ReadParam(4); // before texel xy, after texel xy
    float4 p5 = ReadParam(5); // display mode, label preview, compare-pair-valid

    float split = saturate(p0.x);
    bool before_on_left = p1.x >= 0.5;
    bool before_region = IsBeforeRegion(uv.x, split, before_on_left);
    bool legacy_mode = p5.x >= 0.5;
    bool label_preview = p5.y >= 0.5;
    bool compare_pair_valid = p5.z >= 0.5;

    float2 before_uv = legacy_mode ? LegacyBeforeUV(uv, split, before_on_left) : uv;
    float2 after_uv = legacy_mode ? LegacyAfterUV(uv, split, before_on_left) : uv;

    // Preview can be used before a real pair exists. In that case the add-on
    // binds a snapshot of the current post-effects frame as the After texture.
    float4 color = compare_pair_valid
        ? (before_region ? tex2D(BeforeSampler, before_uv) : tex2D(AfterSampler, after_uv))
        : tex2D(AfterSampler, uv);

    if (compare_pair_valid && p0.w >= 0.5 && p0.y > 0.0 && abs(uv.x - split) <= p0.y)
    {
        float border_alpha = saturate(p0.z);
        color.rgb = lerp(color.rgb, float3(0.95, 0.95, 0.90), border_alpha);
    }

    if (p1.y >= 0.5)
    {
        if (label_preview)
        {
            // Placement preview deliberately ignores divider clipping so both
            // labels remain visible while X/Y/size/outline are adjusted.
            color = BlendLabelBefore(color, uv, p2, p4.xy, saturate(p1.z), max(p1.w, 0.0));
            color = BlendLabelAfter(color, uv, p3, p4.zw, saturate(p1.z), max(p1.w, 0.0));
        }
        else
        {
            // During normal recording each label is clipped to the region it
            // describes, so OFF never floats over ON (or vice versa).
            if (before_region)
                color = BlendLabelBefore(color, uv, p2, p4.xy, saturate(p1.z), max(p1.w, 0.0));
            else
                color = BlendLabelAfter(color, uv, p3, p4.zw, saturate(p1.z), max(p1.w, 0.0));
        }
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
