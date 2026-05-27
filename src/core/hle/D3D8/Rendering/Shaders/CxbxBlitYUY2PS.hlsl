// YUY2 → ARGB pixel shader for PVIDEO overlay compositing.
// The source texture holds raw YUY2 data in R8G8_B8G8_UNORM format:
// Each texel stores a YUY2 macroblock (2 pixels): {Y0, U, Y1, V}
// The texture width is overlayWidth/2 (in macroblocks).
//
// uv.x maps [0,1] across the destination output rectangle.  This shader
// handles both 1:1 and scaled presentation by computing the corresponding
// source pixel coordinate and sampling the correct YUY2 macroblock.

Texture2D<float4> tex : register(t0);
SamplerState samp : register(s0);

float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    float texWidth, texHeight;
    tex.GetDimensions(texWidth, texHeight);

    // Source dimensions in pixels (YUY2 packs 2 pixels per macroblock)
    float srcPixelWidth = texWidth * 2.0;

    // Map output uv → source pixel coordinate
    float srcPixelX = uv.x * srcPixelWidth;
    // Clamp to avoid sampling beyond the last macroblock
    srcPixelX = clamp(srcPixelX, 0.0, srcPixelWidth - 1.0);

    // Macroblock index and intra-macroblock offset
    float srcPixelIdx = floor(srcPixelX);
    uint  macroBlock = uint(srcPixelIdx * 0.5);  // floor(srcPixelIdx / 2)
    bool  isOdd = (uint(srcPixelIdx) & 1) != 0;   // odd = second pixel in macroblock

    // Sample the macroblock at its center (point sampling)
    float blockTC = (float(macroBlock) + 0.5) / texWidth;
    float rowTC = (clamp(uv.y, 0.0, 1.0) * texHeight + 0.5) / texHeight;
    float4 yuyv = tex.SampleLevel(samp, float2(blockTC, rowTC), 0);

    // YUY2 BT.601 → RGB (same coefficients as ____YUY2ToARGBRow_C)
    float y, u, v;
    u = yuyv.g * 255.0 - 128.0;
    v = yuyv.a * 255.0 - 128.0;

    if (isOdd) {
        y = yuyv.b * 255.0 - 16.0;
    } else {
        y = yuyv.r * 255.0 - 16.0;
    }

    float r = 1.1644 * y            + 1.5960 * v;
    float g = 1.1644 * y - 0.3918 * u - 0.8130 * v;
    float b = 1.1644 * y + 2.0172 * u;

    return float4(saturate(r / 255.0), saturate(g / 255.0), saturate(b / 255.0), 1.0);
}
