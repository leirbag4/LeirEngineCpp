// Sprite.vert.wgsl - 2D sprite vertex shader (Fase 5, WebGPU backend).
// Push constants emulated as a uniform buffer at group 1 (= setLayouts.size()).

struct VSOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) fragTexCoord: vec2<f32>,
};

// std430: mvp@0, color@64, uvRect@80, textureIndex@96 (size 112).
struct SpritePushConstants {
    mvp: mat4x4<f32>,
    color: vec4<f32>,
    uvRect: vec4<f32>,
    textureIndex: u32,
};

@group(1) @binding(0) var<uniform> push: SpritePushConstants;

@vertex
fn vs_main(
    @location(0) inPosition: vec2<f32>,
    @location(1) inTexCoord: vec2<f32>,
) -> VSOutput {
    var out: VSOutput;
    out.position = push.mvp * vec4<f32>(inPosition, 0.0, 1.0);
    out.fragTexCoord = inTexCoord;
    return out;
}