// Grid.vert.slang - editor ground grid vertex shader (procedural fragment grid).
// The grid is ONE flat quad; the LINES are generated in the FRAGMENT shader
// (distance to the nearest grid line via fwidth). This vertex shader only
// passes the world position through; the UBO carries the view projection, the
// push constants the per-fragment LOD fade controls.

struct VSInput {
    [[vk::location(0)]] float3 inPosition : POSITION0;
};

struct VSOutput {
    [[vk::location(0)]] float3 worldPos : TEXCOORD0;
    float4 position : SV_Position;
};

[[vk::binding(0, 0)]]
cbuffer UniformBufferObject {
    float4x4 viewProjection;
} ubo;

// Declared in BOTH stages so the merged push-constant range covers
// VertexFragment (the Vulkan executor pushes with the combined stage flags; a
// Vertex-only range here would trip VUID-vkCmdPushConstants-offset-01796).
struct GridPushConstants {
    float lineWidth;   // 0  base line width in screen pixels
    float chunkWidth;  // 4  chunk line width in screen pixels
    float pad0;        // 8
    float pad1;        // 12
    float3 cameraPos;  // 16 camera world position (fragment LOD fade)
    float pad2;        // 28
    float4 baseColor;  // 32 grid: thin line color
    float4 chunkColor; // 48 grid: chunk line color (every 10th, brighter)
};

[[vk::push_constant]]
GridPushConstants push : register(b1, space0);

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput output;
    output.worldPos = input.inPosition;
    output.position = mul(ubo.viewProjection, float4(input.inPosition, 1.0));
    return output;
}