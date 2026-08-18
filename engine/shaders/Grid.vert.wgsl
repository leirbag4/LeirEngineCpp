// Grid.vert.wgsl - editor ground grid vertex shader (WebGPU backend).
// Mirrors Grid.vert.slang: UBO at group 0 (viewProjection only — the fragment
// stage must never read it on D3D12), push constants emulated as a uniform
// buffer at group 1 (= setLayouts.size() for the grid pipeline, which only has
// set 0). NDC is D3D-style (WebGPU y-up): positive viewport, no flip. The
// lines are generated in the fragment shader; this stage only passes the world
// position through.

struct VSOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) worldPos: vec3<f32>,
};

struct UniformBufferObject {
    viewProjection: mat4x4<f32>,
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

@group(0) @binding(0) var<uniform> ubo: UniformBufferObject;
@group(1) @binding(0) var<uniform> push: PushConstants;

@vertex
fn vs_main(@location(0) inPosition: vec3<f32>) -> VSOutput {
    var out: VSOutput;
    out.worldPos = inPosition;
    out.position = ubo.viewProjection * vec4<f32>(inPosition, 1.0);
    return out;
}