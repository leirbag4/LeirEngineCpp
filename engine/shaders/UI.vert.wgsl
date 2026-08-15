// UI.vert.wgsl - UI quad vertex shader (Fase 5, WebGPU backend).
// Push constants (screen size) emulated as a uniform buffer at group 1.
// UI logical coords are top-left origin, y down; mapped to D3D-style NDC
// (y-up, top = +1) exactly like the DXIL path: (1 - y/h) * 2 - 1.

struct VSOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) fragTexCoord: vec2<f32>,
    @location(1) fragColor: vec4<f32>,
    @location(2) fragTexIndex: f32,
};

struct PushConstants {
    screenSize: vec2<f32>,
};

@group(1) @binding(0) var<uniform> push: PushConstants;

@vertex
fn vs_main(
    @location(0) inPosition: vec2<f32>,
    @location(1) inTexCoord: vec2<f32>,
    @location(2) inColor: vec4<f32>,
    @location(3) inTexIndex: f32,
) -> VSOutput {
    var out: VSOutput;
    let ndc = vec2<f32>(
        (inPosition.x / push.screenSize.x) * 2.0 - 1.0,
        (1.0 - inPosition.y / push.screenSize.y) * 2.0 - 1.0
    );
    out.position = vec4<f32>(ndc, 0.0, 1.0);
    out.fragTexCoord = inTexCoord;
    out.fragColor = inColor;
    out.fragTexIndex = inTexIndex;
    return out;
}