struct PSInput {
    [[vk::location(0)]] float2 fragTexCoord : TEXCOORD0;
};

// Bindless texture table (set 0). Index from the push constant (uniform).
// The WEB (browser, naga) cannot compile binding_array, so LEIR_BINDLESS=0
// degrades to a single texture bound per draw.
#ifndef LEIR_BINDLESS
#define LEIR_BINDLESS 1
#endif

#if LEIR_BINDLESS
[[vk::binding(0, 0)]]
Sampler2D textures[];
#else
[[vk::binding(0, 0)]]
Sampler2D tex;
#endif

struct SpritePushConstants {
    float4x4 mvp;
    float4 color;
    float4 uvRect;
    uint textureIndex;
};

[[vk::push_constant]]
SpritePushConstants push : register(b1, space0);

[shader("fragment")]
float4 main(PSInput input) : SV_Target {
    float2 uv = push.uvRect.xy + input.fragTexCoord * push.uvRect.zw;
#if LEIR_BINDLESS
    return textures[push.textureIndex].Sample(uv) * push.color;
#else
    return tex.Sample(uv) * push.color;
#endif
}