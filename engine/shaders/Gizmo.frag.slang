// Gizmo.frag.slang - pass-through color for the gizmo line renderer. The shape
// and constant pixel width are produced by Gizmo.vert.slang.

struct PSInput {
    [[vk::location(0)]] float4 fragColor : COLOR0;
};

[shader("fragment")]
float4 main(PSInput input) : SV_Target {
    return input.fragColor;
}
