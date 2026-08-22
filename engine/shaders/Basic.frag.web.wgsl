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
@binding(0) @group(1) var tex_texture_0 : texture_2d<f32>;

@binding(1) @group(1) var tex_sampler_0 : sampler;

struct pixelOutput_0
{
    @location(0) output_0 : vec4<f32>,
};

struct pixelInput_0
{
    @location(0) fragNormal_0 : vec3<f32>,
    @location(1) fragTexCoord_0 : vec2<f32>,
    @location(2) fragWorldPos_0 : vec3<f32>,
};

@fragment
fn ps_main( _S1 : pixelInput_0) -> pixelOutput_0
{
    var diffuse_0 : vec3<f32> = push_0.lightColor_0 * vec3<f32>(max(dot(normalize(_S1.fragNormal_0), (vec3<f32>(0) - normalize(push_0.lightDir_0))), 0.0f));
    var ambient_0 : vec3<f32> = push_0.ambientColor_0;
    ;
    var baseColor_0 : vec4<f32> = (textureSample((tex_texture_0), (tex_sampler_0), (_S1.fragTexCoord_0))) * push_0.color_0;
    var _S2 : pixelOutput_0 = pixelOutput_0( vec4<f32>((ambient_0 + diffuse_0) * baseColor_0.xyz, baseColor_0.w) );
    return _S2;
}

