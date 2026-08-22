// GizmoSolid.frag.slang - editor gizmo SOLID geometry fragment shader.
// Pass-through color (alpha comes from the vertex color: opaque cones/cubes,
// translucent plane squares). Blending is enabled on the pipeline so the
// translucent area-squares blend against the scene.

struct PSInput {
    [[vk::location(0)]] float4 fragColor : COLOR0;
};

[shader("fragment")]
float4 main(PSInput input) : SV_Target {
    return input.fragColor;
}