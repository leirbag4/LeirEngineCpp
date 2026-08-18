// Gizmo.vert.slang - procedural constant-pixel-width 3D line rendering.
//
// Each line is a 4-corner triangle strip; every corner carries the full
// segment (start, end, color) plus which corner it is (cornerX: 0=start /
// 1=end, cornerY: side +1/-1). The vertex shader projects both endpoints,
// computes the perpendicular of the line in screen PIXELS and expands it by
// width/2 pixels, so the line is `width` px thick at any distance/angle and
// perspective-correct (clip w/z are interpolated between the endpoints).
//
// This is the standard GPU-side line technique used by engine gizmo/debug
// renderers (no geometry shaders -> portable to WebGPU).

struct VSInput {
    [[vk::location(0)]] float3 start : POSITION0;
    [[vk::location(1)]] float3 end : TEXCOORD0;
    [[vk::location(2)]] float4 color : COLOR0;
    [[vk::location(3)]] float cornerX : TEXCOORD1;
    [[vk::location(4)]] float cornerY : TEXCOORD2;
    [[vk::location(5)]] float width : TEXCOORD3;
};

struct VSOutput {
    [[vk::location(0)]] float4 fragColor : COLOR0;
    // Signed perpendicular offset from the segment centerline in PIXELS,
    // interpolated across the quad. The fragment shader uses it for the 1px
    // anti-aliasing ramp.
    [[vk::location(1)]] float sidePx : TEXCOORD1;
    // Line width in pixels, interpolated (per-line, uniform across the quad).
    [[vk::location(2)]] float widthPx : TEXCOORD2;
    float4 position : SV_Position;
};

[[vk::binding(0, 0)]]
cbuffer UniformBufferObject {
    float4x4 viewProjection;
} ubo;

// Declared only in the vertex stage (the executor pushes with Vertex); the
// fragment stage is a pass-through.
struct GizmoPushConstants {
    float viewportWidth;
    float viewportHeight;
    float pad0;
    float pad1;
};

[[vk::push_constant]]
GizmoPushConstants push : register(b1, space0);

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput output;

    float4 clipS = mul(ubo.viewProjection, float4(input.start, 1.0));
    float4 clipE = mul(ubo.viewProjection, float4(input.end, 1.0));
    float2 ndcS = clipS.xy / clipS.w;
    float2 ndcE = clipE.xy / clipE.w;

    float vw = max(push.viewportWidth, 1.0);
    float vh = max(push.viewportHeight, 1.0);

    float t = input.cornerX;
    float w = lerp(clipS.w, clipE.w, t);
    float z = lerp(clipS.z, clipE.z, t);

    // Line direction in screen pixels (isotropic), then a unit perpendicular.
    float2 dirPx = (ndcE - ndcS) * float2(0.5 * vw, 0.5 * vh);
    float2 perp = float2(-dirPx.y, dirPx.x);
    float dirLen = length(perp);
    perp = dirLen > 1e-6 ? perp / dirLen : float2(0.0, 0.0);

    // Expand by halfQuad pixels along the perpendicular and convert back to
    // NDC. halfQuad = max(width, 1.0)/2: the quad is NEVER thinner than 1 px,
    // otherwise a sub-pixel quad breaks up into gaps under pixel-center
    // rasterization (no MSAA). The fragment shader scales the alpha by the
    // REAL width to render sub-pixel lines faint and continuous.
    float halfQuad = max(input.width, 1.0) * 0.5;
    float2 offsetPx = perp * (halfQuad * input.cornerY);
    float2 offsetNdc = offsetPx / float2(0.5 * vw, 0.5 * vh);

    float2 ndc = lerp(ndcS, ndcE, t) + offsetNdc;
    output.position = float4(ndc * w, z, w);
    output.fragColor = input.color;
    output.sidePx = halfQuad * input.cornerY;
    output.widthPx = input.width;
    return output;
}