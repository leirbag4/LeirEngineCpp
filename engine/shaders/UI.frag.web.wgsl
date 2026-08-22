@binding(0) @group(0) var tex_texture_0 : texture_2d<f32>;

@binding(1) @group(0) var tex_sampler_0 : sampler;

struct pixelOutput_0
{
    @location(0) output_0 : vec4<f32>,
};

struct pixelInput_0
{
    @location(0) fragTexCoord_0 : vec2<f32>,
    @location(2) fragColor_0 : vec4<f32>,
    @location(1) fragTexIndex_0 : f32,
};

@fragment
fn ps_main( _S1 : pixelInput_0) -> pixelOutput_0
{
    ;
    var _S2 : pixelOutput_0 = pixelOutput_0( (textureSample((tex_texture_0), (tex_sampler_0), (_S1.fragTexCoord_0))) * _S1.fragColor_0 );
    return _S2;
}

