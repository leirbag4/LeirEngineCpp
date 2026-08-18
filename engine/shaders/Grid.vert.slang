// Grid.vert.slang - editor ground grid vertex shader (procedural fragment grid).
// The grid is one flat quad per LOD level; the LINES are generated in the
// FRAGMENT shader (distance to the nearest grid line via fwidth). This vertex
// shader only passes the world position through; the UBO carries the view
// projection + camera position for the per-fragment LOD fade.

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

// Per-level procedural controls. Declared in BOTH stages so the merged
// push-constant range covers VertexFragment (the Vulkan executor pushes with
// the combined stage flags; a Vertex-only range here would trip
// VUID-vkCmdPushConstants-offset-01796).
struct GridPushConstants {
    float unit;        // line spacing of this level (1 / 10 / 100)
    float fadeStart;   // LOD fade-in distance
    float fadeEnd;     // LOD fade-out distance
    float lineWidth;   // line width in screen pixels
    float mode;        // 0 = grid level, 1 = axis quad
    float pad1;
    float pad2;
    float pad3;
    float3 cameraPos;  // camera world position (fragment LOD fade)
    float pad4;
    float4 baseColor;   // grid: thin line color / axis: X-axis color
    float4 chunkColor;  // grid: chunk line color / axis: Z-axis color
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
