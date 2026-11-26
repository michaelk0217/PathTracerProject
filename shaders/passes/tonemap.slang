// accumulation (linear HDR) -> display (sRGB-encoded LDR)

#include "common/bindings.slangh"


// Narkowicz 2015 ACES approximation
float3 acesFitted(float3 x) 
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float3 linearToSrgb(float3 c)
{
    return select(c <= 0.0031308, c * 12.92, 1.055 * pow(c, 1.0 / 2.4) - 0.055);
}

// ai generated
// Hue-preserving: compress the peak channel with the ACES curve, scale RGB uniformly.
// Bright saturated lights stay saturated instead of clipping to white.
float3 tonemapHuePreserving(float3 hdr)
{
    float peak = max(hdr.r, max(hdr.g, hdr.b));
    if (peak <= 0.0) return float3(0.0);
    float mapped = acesFitted(float3(peak)).r;
    return hdr * (mapped / peak);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void tonemap(uint3 tid: SV_DispatchThreadID)
{
    uint2 pixel = tid.xy;
    if (any(pixel >= pc.extent)) return;
    FrameUniforms frame = pc.frame[0];
    // float3 hdr = gImages[kSlotAccumulation][pixel].rgb * frame.exposure;
    // gImages[kSlotDisplay][pixel] = float4(linearToSrgb(acesFitted(hdr)), 1.0);

    float3 hdr = gImages[kSlotAccumulation][pixel].rgb * frame.exposure;
    float3 ldr = frame.tonemapper == 1 ? tonemapHuePreserving(hdr) : acesFitted(hdr);
    gImages[kSlotDisplay][pixel] = float4(linearToSrgb(ldr), 1.0);
    
}