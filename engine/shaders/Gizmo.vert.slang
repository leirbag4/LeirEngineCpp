// Gizmo.vert.slang - procedural constant-pixel-width 3D line rendering.
//
// Each line is a 4-corner triangle strip; every corner carries the full
// segment (start, end, color) plus which corner it is (cornerX: 0=start /
// 1=end, cornerY: side +1/-1). The vertex shader projects both endpoints,
// computes the perpendicular of the line in screen space and expands it by
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

    // Screen-space uniform units (1 unit = 1 px in X and Y alike) so the
    // perpendicular and the pixel width are angle-independent.
    float2 scale = float2(push.viewportHeight / push.viewportWidth, 1.0);
    float2 invScale = float2(push.viewportWidth / push.viewportHeight, 1.0);
    float2 sS = ndcS * scale;
    float2 sE = ndcE * scale;

    float t = input.cornerX;
    float w = lerp(clipS.w, clipE.w, t);
    float z = lerp(clipS.z, clipE.z, t);

    float2 dir = sE - sS;
    float2 perp = float2(0.0, 0.0);
    if (length(dir) > 1e-6)
        perp = normalize(float2(-dir.y, dir.x));

    float2 expanded = lerp(sS, sE, t) + perp * (input.width * 0.5 * input.cornerY);
    output.position = float4(expanded * invScale * w, z, w);
    output.fragColor = input.color;
    return output;
}
