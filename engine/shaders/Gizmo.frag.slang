// Gizmo.frag.slang - color + 1px anti-aliased edges for the gizmo line
// renderer. The constant-pixel-width quad shape is produced by Gizmo.vert.slang;
// this shader only applies the alpha ramp from the interpolated perpendicular
// offset (sidePx, in pixels).

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
    float a = 1.0 - smoothstep(halfCore, halfCore + 1.0, abs(input.sidePx));
    return float4(input.fragColor.rgb, input.fragColor.a * a);
}
