// Grid.frag.slang - editor ground grid fragment shader.
//
// Procedural grid, the canonical technique used by Unity/Unreal: the lines are
// NOT geometry. For each pixel we measure the distance from the fragment's
// world position to the nearest grid line and convert it to SCREEN PIXELS via
// fwidth(worldPos), so every line has a constant pixel width at any distance
// and angle (no foreshortening -> no "paper ribbon" bands and no vanishing
// lines). Edges are anti-aliased with a 1px smoothstep ramp.
//
// Per level: thin lines every `unit`, chunk lines every 10*unit drawn slightly
// brighter (Unity's 10x10 emphasis). The whole level fades in/out by horizontal
// camera distance (LOD); overlapping levels dissolve into each other (lines at
// multiples of 10 stay alive through the sum of alphas). mode=1 draws the
// origin axes instead (red along X, blue along Z, constant screen width).

struct PSInput {
    [[vk::location(0)]] float3 worldPos : TEXCOORD0;
};

struct GridPushConstants {
    float unit;
    float fadeStart;
    float fadeEnd;
    float lineWidth;
    float mode;
    float pad1;
    float pad2;
    float pad3;
    float3 cameraPos;
    float pad4;
    float4 baseColor;
    float4 chunkColor;
};

// NOTE: no cbuffer here — the set-0 UBO is vertex-stage only on D3D12 (the
// backend root-signature binds it with VERTEX visibility), so the fragment
// shader must never read it. cameraPos lives in the push constants instead.

[[vk::push_constant]]
GridPushConstants push : register(b1, space0);

// Distance (in grid units) to the nearest line of the given spacing, converted
// to screen pixels via fwidth and anti-aliased: 1.0 exactly on the line,
// ramping to 0.0 over `width` pixels. Returns alpha coverage.
float GridLineAlpha(float2 worldXZ, float spacing) {
    float2 p = worldXZ / spacing;
    float2 dist = abs(fract(p - 0.5) - 0.5) / fwidth(p); // pixels to nearest line
    return 1.0 - min(min(dist.x, dist.y) / push.lineWidth, 1.0);
}

[shader("fragment")]
float4 main(PSInput input) : SV_Target {
    // LOD fade by horizontal distance to the camera (axis mode never fades).
    float fade = 1.0;
    if (push.fadeEnd > push.fadeStart) {
        float d = length(push.cameraPos.xz - input.worldPos.xz);
        fade = 1.0 - smoothstep(push.fadeStart, push.fadeEnd, d);
    }

    float alpha;
    float3 color;
    if (push.mode > 0.5) {
        // Axis quad: red where |z| < width px (the X axis), blue where |x| is.
        float ax = abs(input.worldPos.x) / fwidth(input.worldPos.x);
        float az = abs(input.worldPos.z) / fwidth(input.worldPos.z);
        float axisX = 1.0 - min(az / push.lineWidth, 1.0);
        float axisZ = 1.0 - min(ax / push.lineWidth, 1.0);
        color = lerp(push.chunkColor.rgb, push.baseColor.rgb, axisX);
        alpha = max(axisX, axisZ);
    } else {
        // Level quad: thin lines every `unit`, chunk lines every 10*unit.
        float2 xz = input.worldPos.xz;
        float line = GridLineAlpha(xz, push.unit);
        float chunk = GridLineAlpha(xz, push.unit * 10.0);
        color = lerp(push.baseColor.rgb, push.chunkColor.rgb, chunk);
        alpha = line;
    }

    return float4(color * fade, alpha * fade);
}
