// Grid.frag.slang - color + 1px anti-aliased edges for the editor ground grid.
// Mirrors Gizmo.frag.slang (the "Test2" line technique): the constant-pixel-
// width quad shape is produced by Grid.vert.slang; this shader only applies the
// alpha ramp from the interpolated perpendicular offset (sidePx, in pixels).
// Per-line transparency (the Unity-style distance fade of the minor lines) is
// baked into fragColor.a on the CPU per line.

struct PSInput {
    [[vk::location(0)]] float4 fragColor : COLOR0;
    [[vk::location(1)]] float sidePx : TEXCOORD1;
    [[vk::location(2)]] float widthPx : TEXCOORD2;
};

[shader("fragment")]
float4 main(PSInput input) : SV_Target {
    // Fully opaque core = width/2 - 0.5px from the centerline, then a 1px
    // linear-ish fade (smoothstep) to the quad edge (width/2 + 0.5px).
    float halfCore = max(input.widthPx * 0.5 - 0.5, 0.0);
    float a;
    if (input.widthPx < 1.0) {
        // Sub-pixel line: the VS quad is clamped to 1px (no rasterization
        // gaps), so sidePx spans [-0.5, 0.5]. Scale the 1px fade by the REAL
        // width so a 0.5px line still renders visibly THINNER than 1px.
        a = input.widthPx * (1.0 - smoothstep(0.0, 1.0, abs(input.sidePx)));
    } else {
        a = 1.0 - smoothstep(halfCore, halfCore + 1.0, abs(input.sidePx));
    }
    return float4(input.fragColor.rgb, input.fragColor.a * a);
}