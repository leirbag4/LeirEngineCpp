// Basic.vert.wgsl - 3D mesh vertex shader (Fase 5, WebGPU backend).
// Mirrors Basic.vert.slang: UBO at group 0 (viewProjection), bindless table is
// declared in the fragment stage, push constants emulated as a uniform buffer
// at group 2 (= setLayouts.size() for the Basic pipeline). NDC is D3D-style
// (WebGPU y-up): positive viewport, no flip.

struct VSOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) fragNormal: vec3<f32>,
    @location(1) fragTexCoord: vec2<f32>,
    @location(2) fragWorldPos: vec3<f32>,
};

struct UniformBufferObject {
    viewProjection: mat4x4<f32>,
};

// WGSL uniform layout auto-aligns vec3 to 16 bytes, reproducing the std430
// offsets the C++ side writes: lightDir@0, lightColor@16, ambientColor@32,
// color@48, model@64, textureIndex@128 (struct size 144).
struct PushConstants {
    lightDir: vec3<f32>,
    lightColor: vec3<f32>,
    ambientColor: vec3<f32>,
    color: vec4<f32>,
    model: mat4x4<f32>,
    textureIndex: u32,
};

@group(0) @binding(0) var<uniform> ubo: UniformBufferObject;
@group(2) @binding(0) var<uniform> push: PushConstants;

@vertex
fn vs_main(
    @location(0) inPosition: vec3<f32>,
    @location(1) inNormal: vec3<f32>,
    @location(2) inTexCoord: vec2<f32>,
) -> VSOutput {
    var out: VSOutput;
    let worldPos = push.model * vec4<f32>(inPosition, 1.0);
    out.position = ubo.viewProjection * worldPos;
    // WGSL has no mat4->mat3 cast; build the 3x3 upper-left from columns.
    out.fragNormal = mat3x3<f32>(push.model[0].xyz, push.model[1].xyz, push.model[2].xyz) * inNormal;
    out.fragTexCoord = inTexCoord;
    out.fragWorldPos = worldPos.xyz;
    return out;
}