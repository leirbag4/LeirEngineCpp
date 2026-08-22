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
@binding(0) @group(0) var tex_texture_0 : texture_2d<f32>;

@binding(1) @group(0) var tex_sampler_0 : sampler;

struct pixelOutput_0
{
    @location(0) output_0 : vec4<f32>,
};

struct pixelInput_0
{
    @location(0) fragTexCoord_0 : vec2<f32>,
};

@fragment
fn ps_main( _S1 : pixelInput_0) -> pixelOutput_0
{
    var uv_0 : vec2<f32> = push_0.uvRect_0.xy + _S1.fragTexCoord_0 * push_0.uvRect_0.zw;
    ;
    var _S2 : pixelOutput_0 = pixelOutput_0( (textureSample((tex_texture_0), (tex_sampler_0), (uv_0))) * push_0.color_0 );
    return _S2;
}

