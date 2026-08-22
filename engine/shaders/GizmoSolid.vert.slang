// GizmoSolid.vert.slang - editor gizmo SOLID geometry vertex shader.
//
// Filled transform-gizmo primitives (arrow cones, scale cubes, translucent
// translate plane-squares) that the line-only GizmoRenderer cannot draw. The
// CPU generates plain triangles each frame (TransformGizmo) with per-vertex
// position + color; this shader only transforms by the shared viewProjection
// UBO. Depth is tested against the scene but NOT written, so gizmo solids
// overlay the geometry without polluting the depth buffer.

struct VSInput {
    [[vk::location(0)]] float3 position : POSITION0;
    [[vk::location(1)]] float4 color : COLOR0;
};

struct VSOutput {
    [[vk::location(0)]] float4 fragColor : COLOR0;
    float4 position : SV_Position;
};

[[vk::binding(0, 0)]]
cbuffer UniformBufferObject {
    float4x4 viewProjection;
} ubo;

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(ubo.viewProjection, float4(input.position, 1.0));
    output.fragColor = input.color;
    return output;
}