// Grid.frag.slang - color + 1px anti-aliased edges + PER-PIXEL DISTANCE FADE
// (fog by depth) for the editor ground grid. Mirrors Gizmo.frag.slang (the
// "Test2" line technique): the constant-pixel-width quad shape is produced by
// Grid.vert.slang; this shader applies the alpha ramp from the interpolated
// perpendicular offset (sidePx) AND the Unity-style distance fade computed from
// the interpolated view depth (depth, fed by the vertex shader).
//
// Why in the shader (not CPU): every line is a SINGLE quad (no CPU
// subdivision), the fade is exact per pixel and the vertex count stays ~1 quad
// per line. The CPU only bakes color.a = the level's role alpha (the line's max
// opacity) and `spacing` (the level, in world units; 0 = opaque reference line).

struct PSInput {
    [[vk::location(0)]] float4 fragColor : COLOR0;
    [[vk::location(1)]] float sidePx : TEXCOORD1;
    [[vk::location(2)]] float widthPx : TEXCOORD2;
    // View depth (clip.w) interpolated across the quad (see Grid.vert.slang).
    [[vk::location(3)]] float depth : TEXCOORD3;
    // Level spacing (0 = opaque line: skip the distance fade).
    [[vk::location(4)]] float spacing : TEXCOORD4;
};

// Shared with the vertex stage (the executor pushes with Vertex|Fragment); the
// layout must match the C++ GridPushConstants.
struct GridPushConstants {
    float viewportWidth;
    float viewportHeight;
    float scale;            // px per unit at depth 1 (vh * 0.5 * f)
    float fadeStart;        // cell-size fade band (px of cell)
    float fadeEnd;
    float horizonStart;     // horizon fade band (view depth, units)
    float horizonEnd;
    float overrideDensity;  // >=0: manual mode (uniform density, no horizon fade)
};

[[vk::push_constant]]
GridPushConstants push : register(b1, space0);

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

    // Per-pixel distance fade (fog by depth): the line dissolves as the view
    // depth grows. fadeCell (cell-size, Unity-style) + fadeHorizon, min()ed.
    // spacing <= 0 (origin axes) skips the fade entirely; manual mode
    // (overrideDensity >= 0) uses the uniform override density and skips the
    // horizon fade (it is a pure LOD transition test, camera-independent).
    float depth = max(input.depth, 0.1); // near-plane guard
    float fade = 1.0;
    if (input.spacing > 0.0) {
        float density = (push.overrideDensity >= 0.0)
            ? push.overrideDensity
            : push.scale / depth;
        float cellPx = input.spacing * density;
        float fadeCell = smoothstep(push.fadeStart, push.fadeEnd, cellPx);
        float fadeHorizon = (push.overrideDensity >= 0.0)
            ? 1.0
            : 1.0 - smoothstep(push.horizonStart, push.horizonEnd, depth);
        fade = min(fadeCell, fadeHorizon);
    }
    return float4(input.fragColor.rgb, input.fragColor.a * a * fade);
}