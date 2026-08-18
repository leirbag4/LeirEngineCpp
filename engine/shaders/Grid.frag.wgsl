// Grid.frag.wgsl - editor ground grid fragment shader (WebGPU backend).
// Mirrors Grid.frag.slang: a single quad, procedural lines per-fragment from
// the distance to the nearest line, converted to screen pixels with the
// perpendicular screen gradient length(dpdx, dpdy) so the line width is
// constant on screen at any distance/angle (2x2 super-sampled -> continuous
// anti-aliased lines). L1/L10/L100 evaluated and blended in one pass (no
// per-LOD quad stacking -> no z-fighting on shared chunk lines). Chunk lines
// (every 10x a level) drawn in the brighter chunk color; origin axes red X /
// blue Z 2px.

struct PSInput {
    @location(0) worldPos: vec3<f32>,
};

struct PushConstants {
    lineWidth: f32,   // 0
    chunkWidth: f32,  // 4
    pad0: f32,        // 8
    pad1: f32,        // 12
    cameraPos: vec3<f32>, // 16
    pad2: f32,        // 28
    baseColor: vec4<f32>, // 32
    chunkColor: vec4<f32>, // 48
};

// NOTE: no UBO here — the set-0 uniform is vertex-stage only on D3D12 (the
// backend root signature binds it with VERTEX visibility), so the fragment
// shader must never read it. cameraPos lives in the push constants.

@group(1) @binding(0) var<uniform> push: PushConstants;

// Distance to the nearest grid line of the given spacing, in screen pixels,
// anti-aliased with a SMOOTH profile (1.0 on the line, fading to 0 over
// [width*0.5, width*1.5] px). The pixel distance is measured with the
// PERPENDICULAR screen-space gradient length(dpdx, dpdy) of the world field —
// NOT fwidth() (the |dpdx|+|dpdy| SUM), which overestimates the derivative on
// oblique lines and thickens them / smears the axes into wide bands. The
// distance is then 2x2 SUPER-SAMPLED (4 quarter-pixel sub-positions, averaged):
// a sub-pixel/slanted line sampled once per pixel oscillates between covered
// and uncovered pixels -> dotted particles; averaging keeps it continuous,
// like MSAA.
fn gridLineAlpha(worldXZ: vec2<f32>, spacing: f32, widthPx: f32) -> f32 {
    let p = worldXZ / spacing;
    let grad = vec2<f32>(length(vec2<f32>(dpdx(p.x), dpdy(p.x))),
                         length(vec2<f32>(dpdx(p.y), dpdy(p.y))));
    let gradInv = vec2<f32>(1.0 / max(grad.x, 1e-6), 1.0 / max(grad.y, 1e-6));
    const offsets = array<vec2<f32>, 4>(
        vec2<f32>(-0.25, -0.25), vec2<f32>(0.25, -0.25),
        vec2<f32>(-0.25, 0.25),  vec2<f32>(0.25, 0.25));
    var a: f32 = 0.0;
    for (var i = 0; i < 4; i = i + 1) {
        let sub = p + offsets[i] * grad;
        let cell = abs(fract(sub - 0.5) - 0.5);
        let d = min(cell.x * gradInv.x, cell.y * gradInv.y);
        a += 1.0 - smoothstep(widthPx * 0.5, widthPx * 1.5, d);
    }
    return a * 0.25;
}

// Coverage of an origin axis (world == 0), 2px wide, supersampled like the
// grid lines so the axes stay a crisp continuous line at any angle.
fn axisLineAlpha(world: f32, grad: vec2<f32>) -> f32 {
    const offsets = array<vec2<f32>, 4>(
        vec2<f32>(-0.25, -0.25), vec2<f32>(0.25, -0.25),
        vec2<f32>(-0.25, 0.25),  vec2<f32>(0.25, 0.25));
    var a: f32 = 0.0;
    for (var i = 0; i < 4; i = i + 1) {
        let sub = world + offsets[i].x * grad.x + offsets[i].y * grad.y;
        let d = abs(sub) / max(length(grad), 1e-6);
        a += 1.0 - smoothstep(0.5, 1.5, d);
    }
    return a * 0.25;
}

@fragment
fn ps_main(input: PSInput) -> @location(0) vec4<f32> {
    let d = distance(push.cameraPos.xz, input.worldPos.xz);

    // On-screen density: pixels per world unit (worst of the two line families),
    // computed from the PERPENDICULAR screen gradient (sqrt(dpdx^2+dpdy^2)) —
    // NOT fwidth() (the |dpdx|+|dpdy| SUM), which overestimates the derivative
    // where the floor converges (dpdy(worldX) explodes near the vanishing point)
    // and collapses pxPerUnit to ~0 -> the density clamp dissolves every level
    // and the near floor ends up empty. With the true gradient, a level only
    // fades once its lines are genuinely closer than ~2-4 px.
    let g = vec2<f32>(length(vec2<f32>(dpdx(input.worldPos.x), dpdy(input.worldPos.x))),
                      length(vec2<f32>(dpdx(input.worldPos.z), dpdy(input.worldPos.z))));
    let pxPerUnit = max(g.x, g.y);

    // Per-level visibility: distance LOD times density clamp (smooth 2-4px).
    let v1 = (1.0 - smoothstep(30.0, 80.0, d)) * smoothstep(2.0, 4.0, pxPerUnit * 1.0);
    let v10 = (1.0 - smoothstep(60.0, 600.0, d)) * smoothstep(2.0, 4.0, pxPerUnit * 10.0);
    let v100 = (1.0 - smoothstep(500.0, 8000.0, d)) * smoothstep(2.0, 4.0, pxPerUnit * 100.0);

    let xz = input.worldPos.xz;
    var color = vec3<f32>(0.0, 0.0, 0.0);
    var cov: f32 = 0.0;

    // L1: lines every 1, chunk lines every 10.
    {
        let line = gridLineAlpha(xz, 1.0, push.lineWidth);
        let chunk = gridLineAlpha(xz, 10.0, push.chunkWidth);
        let c = max(line, chunk) * v1;
        color += mix(push.baseColor.rgb, push.chunkColor.rgb, chunk) * c;
        cov += c;
    }
    // L10: lines every 10, chunk lines every 100.
    {
        let line = gridLineAlpha(xz, 10.0, push.lineWidth);
        let chunk = gridLineAlpha(xz, 100.0, push.chunkWidth);
        let c = max(line, chunk) * v10;
        color += mix(push.baseColor.rgb, push.chunkColor.rgb, chunk) * c;
        cov += c;
    }
    // L100: lines every 100, chunk lines every 1000.
    {
        let line = gridLineAlpha(xz, 100.0, push.lineWidth);
        let chunk = gridLineAlpha(xz, 1000.0, push.chunkWidth);
        let c = max(line, chunk) * v100;
        color += mix(push.baseColor.rgb, push.chunkColor.rgb, chunk) * c;
        cov += c;
    }

    // Origin axes: red X / blue Z, constant 2px, never fading. Suppress the
    // base grid right under them so the colored line reads clean.
    let axisX = axisLineAlpha(input.worldPos.z, vec2<f32>(dpdx(input.worldPos.z), dpdy(input.worldPos.z)));
    let axisZ = axisLineAlpha(input.worldPos.x, vec2<f32>(dpdx(input.worldPos.x), dpdy(input.worldPos.x)));
    let axis = max(axisX, axisZ);
    color *= (1.0 - axis);
    cov *= (1.0 - axis);
    color += vec3<f32>(0.95, 0.25, 0.25) * axisX + vec3<f32>(0.30, 0.55, 1.0) * axisZ;
    cov += axis;

    let a = saturate(cov);
    var rgb = vec3<f32>(0.0, 0.0, 0.0);
    if (a > 1e-5) {
        rgb = color / max(cov, 1e-5);
    }
    return vec4<f32>(rgb, a);
}