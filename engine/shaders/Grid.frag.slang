// Grid.frag.slang - editor ground grid fragment shader.
//
// Procedural grid, the canonical technique used by Unity/Unreal: the lines are
// NOT geometry. For each pixel we measure the distance from the fragment's
// world position to the nearest grid line and convert it to SCREEN PIXELS via
// the perpendicular screen gradient length(ddx, ddy) of the world field, so
// every line has a constant pixel width at any distance and angle (no
// foreshortening -> no "paper ribbon" bands and no vanishing lines). The
// distance is 2x2 super-sampled (4 quarter-pixel sub-samples, averaged) so a
// sub-pixel or slanted line renders continuous instead of dotted.
//
// A SINGLE quad covers the whole grid (no per-LOD quads stacked at the same
// height): L1 (spacing 1, fade 30-80), L10 (spacing 10, fade 60-600) and L100
// (spacing 100, fade 500-8000) are all evaluated per-fragment and blended
// together, so lines that share a position (chunk lines) never z-fight or
// flicker. Every 10th line of a level is drawn in the (brighter) chunk color.
// Each level also fades by ON-SCREEN DENSITY: once its lines are closer than
// ~2-4 px the floor would turn into a solid mass (grazing angles compress
// world units per pixel), so the level dissolves and the next coarser one takes
// over. The origin axes (red X / blue Z, 2px, never fading) are also computed
// here.

struct PSInput {
    [[vk::location(0)]] float3 worldPos : TEXCOORD0;
};

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

// NOTE: no cbuffer here — the set-0 UBO is vertex-stage only on D3D12 (the
// backend root-signature binds it with VERTEX visibility), so the fragment
// shader must never read it. cameraPos lives in the push constants instead.

[[vk::push_constant]]
GridPushConstants push : register(b1, space0);

// Distance (in screen pixels) from `world` to the line world==0, using the
// PERPENDICULAR screen-space gradient length(worldGrad) — NOT fwidth() (the
// |ddx|+|ddy| SUM). The sum overestimates the derivative on oblique lines, so
// dividing by it makes the measured pixel distance too small -> lines render
// thicker than requested and the axes smear into wide bands at grazing angles.
float PxDistToAxis(float world, float2 worldGrad) {
    return abs(world) / max(length(worldGrad), 1e-6);
}

// 2x2 analytic supersampling: evaluate a per-pixel coverage function at 4
// sub-positions (quarter-pixel offsets) and average. A sub-pixel / slanted line
// sampled once per pixel oscillates between covered and uncovered pixels, which
// reads as a dotted string of particles; averaging 4 sub-samples keeps it
// continuous — the fragment-shader equivalent of MSAA for procedural content.
// `worldToPx` converts a quarter-pixel offset to world units (world per pixel).
static float2 kSampleOffsets[4] = { float2(-0.25, -0.25), float2(0.25, -0.25),
                                    float2(-0.25, 0.25),  float2(0.25, 0.25) };

// Distance (in screen pixels) to the nearest grid line of `spacing`, with the
// same gradient-correct conversion and a SMOOTH anti-aliased ramp: 1.0 exactly
// on the line, fading to 0 over [width*0.5, width*1.5] px.
float GridLineAlpha(float2 worldXZ, float spacing, float widthPx) {
    float2 p = worldXZ / spacing;
    // world-per-pixel along each line family's perpendicular (screen) direction
    float2 grad = float2(length(float2(ddx(p.x), ddy(p.x))),
                         length(float2(ddx(p.y), ddy(p.y))));
    float2 gradInv = float2(1.0 / max(grad.x, 1e-6), 1.0 / max(grad.y, 1e-6));
    float a = 0.0;
    [unroll]
    for (int i = 0; i < 4; i++) {
        float2 sub = p + kSampleOffsets[i] * grad;      // quarter-pixel in world units
        float2 cell = abs(fract(sub - 0.5) - 0.5);      // grid units to nearest line
        float d = min(cell.x * gradInv.x, cell.y * gradInv.y);
        a += 1.0 - smoothstep(widthPx * 0.5, widthPx * 1.5, d);
    }
    return a * 0.25;
}

// Coverage of the origin axis (world == 0), 2px wide, supersampled like the
// grid lines so the axes stay a crisp continuous line at any angle.
float AxisLineAlpha(float world, float2 worldGrad) {
    float a = 0.0;
    [unroll]
    for (int i = 0; i < 4; i++) {
        float sub = world + kSampleOffsets[i].x * worldGrad.x
                          + kSampleOffsets[i].y * worldGrad.y;
        float d = PxDistToAxis(sub, worldGrad);
        a += 1.0 - smoothstep(0.5, 1.5, d);
    }
    return a * 0.25;
}

[shader("fragment")]
float4 main(PSInput input) : SV_Target {
    float d = length(push.cameraPos.xz - input.worldPos.xz);

    // On-screen density: pixels per world unit (worst of the two line families),
    // computed from the PERPENDICULAR screen gradient (sqrt(ddx^2+ddy^2)) — NOT
    // fwidth() (the |ddx|+|ddy| SUM). The sum overestimates the derivative where
    // the floor converges (ddy(worldX) explodes near the vanishing point), which
    // collapses pxPerUnit to ~0 and the density clamp dissolves every level —
    // the whole near floor ends up empty. With the true gradient, pxPerUnit is
    // the real on-screen line spacing, so a level only fades once its lines are
    // genuinely closer than ~2-4 px.
    float2 g = float2(length(float2(ddx(input.worldPos.x), ddy(input.worldPos.x))),
                      length(float2(ddx(input.worldPos.z), ddy(input.worldPos.z))));
    float pxPerUnit = max(g.x, g.y);

    // Per-level visibility: distance LOD times density clamp (smooth 2-4px).
    float v1 = (1.0 - smoothstep(30.0, 80.0, d)) * smoothstep(2.0, 4.0, pxPerUnit * 1.0);
    float v10 = (1.0 - smoothstep(60.0, 600.0, d)) * smoothstep(2.0, 4.0, pxPerUnit * 10.0);
    float v100 = (1.0 - smoothstep(500.0, 8000.0, d)) * smoothstep(2.0, 4.0, pxPerUnit * 100.0);

    float2 xz = input.worldPos.xz;
    float3 color = float3(0.0, 0.0, 0.0);
    float cov = 0.0;

    // L1: lines every 1, chunk lines every 10.
    {
        float line = GridLineAlpha(xz, 1.0, push.lineWidth);
        float chunk = GridLineAlpha(xz, 10.0, push.chunkWidth);
        float c = max(line, chunk) * v1;
        color += lerp(push.baseColor.rgb, push.chunkColor.rgb, chunk) * c;
        cov += c;
    }
    // L10: lines every 10, chunk lines every 100.
    {
        float line = GridLineAlpha(xz, 10.0, push.lineWidth);
        float chunk = GridLineAlpha(xz, 100.0, push.chunkWidth);
        float c = max(line, chunk) * v10;
        color += lerp(push.baseColor.rgb, push.chunkColor.rgb, chunk) * c;
        cov += c;
    }
    // L100: lines every 100, chunk lines every 1000.
    {
        float line = GridLineAlpha(xz, 100.0, push.lineWidth);
        float chunk = GridLineAlpha(xz, 1000.0, push.chunkWidth);
        float c = max(line, chunk) * v100;
        color += lerp(push.baseColor.rgb, push.chunkColor.rgb, chunk) * c;
        cov += c;
    }

    // Origin axes: red along X (z==0), blue along Z (x==0), constant 2px width,
    // never fading. The base grid right under the axis is suppressed so the
    // colored line reads clean (no white line bleeding over it).
    float axisX = AxisLineAlpha(input.worldPos.z, float2(ddx(input.worldPos.z), ddy(input.worldPos.z)));
    float axisZ = AxisLineAlpha(input.worldPos.x, float2(ddx(input.worldPos.x), ddy(input.worldPos.x)));
    float axis = max(axisX, axisZ);
    color *= (1.0 - axis);
    cov *= (1.0 - axis);
    color += float3(0.95, 0.25, 0.25) * axisX + float3(0.30, 0.55, 1.0) * axisZ;
    cov += axis;

    float a = saturate(cov);
    float3 rgb = a > 1e-5 ? color / max(cov, 1e-5) : float3(0.0, 0.0, 0.0);
    return float4(rgb, a);
}