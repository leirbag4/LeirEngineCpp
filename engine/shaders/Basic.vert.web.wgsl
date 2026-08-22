struct _MatrixStorage_float4x4std430_0
{
    @align(16) data_0 : array<vec4<f32>, i32(4)>,
};

struct PushConstants_std430_0
{
    @align(16) lightDir_0 : vec3<f32>,
    @align(4) pad0_0 : f32,
    @align(16) lightColor_0 : vec3<f32>,
    @align(4) pad1_0 : f32,
    @align(16) ambientColor_0 : vec3<f32>,
    @align(4) pad2_0 : f32,
    @align(16) color_0 : vec4<f32>,
    @align(16) model_0 : _MatrixStorage_float4x4std430_0,
    @align(16) textureIndex_0 : u32,
};

@group(2) @binding(0) var<uniform> push_0 : PushConstants_std430_0;
struct _MatrixStorage_float4x4std140_0
{
    @align(16) data_1 : array<vec4<f32>, i32(4)>,
};

struct SLANG_ParameterGroup_UniformBufferObject_std140_0
{
    @align(16) viewProjection_0 : _MatrixStorage_float4x4std140_0,
};

@binding(0) @group(0) var<uniform> ubo_0 : SLANG_ParameterGroup_UniformBufferObject_std140_0;
struct VSOutput_0
{
    @location(0) fragNormal_0 : vec3<f32>,
    @location(1) fragTexCoord_0 : vec2<f32>,
    @location(2) fragWorldPos_0 : vec3<f32>,
    @builtin(position) position_0 : vec4<f32>,
};

struct vertexInput_0
{
    @location(0) inPosition_0 : vec3<f32>,
    @location(1) inNormal_0 : vec3<f32>,
    @location(2) inTexCoord_0 : vec2<f32>,
};

@vertex
fn vs_main( _S1 : vertexInput_0) -> VSOutput_0
{
    var worldPos_0 : vec4<f32> = (((mat4x4<f32>(push_0.model_0.data_0[i32(0)][i32(0)], push_0.model_0.data_0[i32(0)][i32(1)], push_0.model_0.data_0[i32(0)][i32(2)], push_0.model_0.data_0[i32(0)][i32(3)], push_0.model_0.data_0[i32(1)][i32(0)], push_0.model_0.data_0[i32(1)][i32(1)], push_0.model_0.data_0[i32(1)][i32(2)], push_0.model_0.data_0[i32(1)][i32(3)], push_0.model_0.data_0[i32(2)][i32(0)], push_0.model_0.data_0[i32(2)][i32(1)], push_0.model_0.data_0[i32(2)][i32(2)], push_0.model_0.data_0[i32(2)][i32(3)], push_0.model_0.data_0[i32(3)][i32(0)], push_0.model_0.data_0[i32(3)][i32(1)], push_0.model_0.data_0[i32(3)][i32(2)], push_0.model_0.data_0[i32(3)][i32(3)])) * (vec4<f32>(_S1.inPosition_0, 1.0f))));
    var output_0 : VSOutput_0;
    output_0.position_0 = (((mat4x4<f32>(ubo_0.viewProjection_0.data_1[i32(0)][i32(0)], ubo_0.viewProjection_0.data_1[i32(0)][i32(1)], ubo_0.viewProjection_0.data_1[i32(0)][i32(2)], ubo_0.viewProjection_0.data_1[i32(0)][i32(3)], ubo_0.viewProjection_0.data_1[i32(1)][i32(0)], ubo_0.viewProjection_0.data_1[i32(1)][i32(1)], ubo_0.viewProjection_0.data_1[i32(1)][i32(2)], ubo_0.viewProjection_0.data_1[i32(1)][i32(3)], ubo_0.viewProjection_0.data_1[i32(2)][i32(0)], ubo_0.viewProjection_0.data_1[i32(2)][i32(1)], ubo_0.viewProjection_0.data_1[i32(2)][i32(2)], ubo_0.viewProjection_0.data_1[i32(2)][i32(3)], ubo_0.viewProjection_0.data_1[i32(3)][i32(0)], ubo_0.viewProjection_0.data_1[i32(3)][i32(1)], ubo_0.viewProjection_0.data_1[i32(3)][i32(2)], ubo_0.viewProjection_0.data_1[i32(3)][i32(3)])) * (worldPos_0)));
    var _S2 : mat4x4<f32> = mat4x4<f32>(push_0.model_0.data_0[i32(0)][i32(0)], push_0.model_0.data_0[i32(0)][i32(1)], push_0.model_0.data_0[i32(0)][i32(2)], push_0.model_0.data_0[i32(0)][i32(3)], push_0.model_0.data_0[i32(1)][i32(0)], push_0.model_0.data_0[i32(1)][i32(1)], push_0.model_0.data_0[i32(1)][i32(2)], push_0.model_0.data_0[i32(1)][i32(3)], push_0.model_0.data_0[i32(2)][i32(0)], push_0.model_0.data_0[i32(2)][i32(1)], push_0.model_0.data_0[i32(2)][i32(2)], push_0.model_0.data_0[i32(2)][i32(3)], push_0.model_0.data_0[i32(3)][i32(0)], push_0.model_0.data_0[i32(3)][i32(1)], push_0.model_0.data_0[i32(3)][i32(2)], push_0.model_0.data_0[i32(3)][i32(3)]);
    output_0.fragNormal_0 = (((_S1.inNormal_0) * (mat3x3<f32>(_S2[i32(0)].xyz, _S2[i32(1)].xyz, _S2[i32(2)].xyz))));
    output_0.fragTexCoord_0 = _S1.inTexCoord_0;
    output_0.fragWorldPos_0 = worldPos_0.xyz;
    return output_0;
}

