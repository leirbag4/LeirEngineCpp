struct _MatrixStorage_float4x4std430_0
{
    @align(16) data_0 : array<vec4<f32>, i32(4)>,
};

struct SpritePushConstants_std430_0
{
    @align(16) mvp_0 : _MatrixStorage_float4x4std430_0,
    @align(16) color_0 : vec4<f32>,
    @align(16) uvRect_0 : vec4<f32>,
    @align(16) textureIndex_0 : u32,
};

@group(1) @binding(0) var<uniform> push_0 : SpritePushConstants_std430_0;
struct VSOutput_0
{
    @location(0) fragTexCoord_0 : vec2<f32>,
    @builtin(position) position_0 : vec4<f32>,
};

struct vertexInput_0
{
    @location(0) inPosition_0 : vec2<f32>,
    @location(1) inTexCoord_0 : vec2<f32>,
};

@vertex
fn vs_main( _S1 : vertexInput_0) -> VSOutput_0
{
    var output_0 : VSOutput_0;
    output_0.position_0 = (((mat4x4<f32>(push_0.mvp_0.data_0[i32(0)][i32(0)], push_0.mvp_0.data_0[i32(0)][i32(1)], push_0.mvp_0.data_0[i32(0)][i32(2)], push_0.mvp_0.data_0[i32(0)][i32(3)], push_0.mvp_0.data_0[i32(1)][i32(0)], push_0.mvp_0.data_0[i32(1)][i32(1)], push_0.mvp_0.data_0[i32(1)][i32(2)], push_0.mvp_0.data_0[i32(1)][i32(3)], push_0.mvp_0.data_0[i32(2)][i32(0)], push_0.mvp_0.data_0[i32(2)][i32(1)], push_0.mvp_0.data_0[i32(2)][i32(2)], push_0.mvp_0.data_0[i32(2)][i32(3)], push_0.mvp_0.data_0[i32(3)][i32(0)], push_0.mvp_0.data_0[i32(3)][i32(1)], push_0.mvp_0.data_0[i32(3)][i32(2)], push_0.mvp_0.data_0[i32(3)][i32(3)])) * (vec4<f32>(_S1.inPosition_0, 0.0f, 1.0f))));
    output_0.fragTexCoord_0 = _S1.inTexCoord_0;
    return output_0;
}

