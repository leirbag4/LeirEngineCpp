@group(1) @binding(0) var<uniform> screenSize_0 : vec2<f32>;
struct VSOutput_0
{
    @location(0) fragTexCoord_0 : vec2<f32>,
    @location(2) fragColor_0 : vec4<f32>,
    @location(1) fragTexIndex_0 : f32,
    @builtin(position) position_0 : vec4<f32>,
};

struct vertexInput_0
{
    @location(0) inPosition_0 : vec2<f32>,
    @location(1) inTexCoord_0 : vec2<f32>,
    @location(2) inColor_0 : vec4<f32>,
    @location(3) inTexIndex_0 : f32,
};

@vertex
fn vs_main( _S1 : vertexInput_0) -> VSOutput_0
{
    var output_0 : VSOutput_0;
    output_0.position_0 = vec4<f32>(vec2<f32>(_S1.inPosition_0.x / screenSize_0.x * 2.0f - 1.0f, (1.0f - _S1.inPosition_0.y / screenSize_0.y) * 2.0f - 1.0f), 0.0f, 1.0f);
    output_0.fragTexCoord_0 = _S1.inTexCoord_0;
    output_0.fragColor_0 = _S1.inColor_0;
    output_0.fragTexIndex_0 = _S1.inTexIndex_0;
    return output_0;
}

