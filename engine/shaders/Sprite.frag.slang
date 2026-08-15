struct PSInput {
    [[vk::location(0)]] float2 fragTexCoord : TEXCOORD0;
};

// Bindless texture table (set 0). Index from the push constant (uniform).
[[vk::binding(0, 0)]]
Sampler2D textures[];

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
    return textures[push.textureIndex].Sample(uv) * push.color;
}