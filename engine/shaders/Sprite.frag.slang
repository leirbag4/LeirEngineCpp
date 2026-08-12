struct PSInput {
    [[vk::location(0)]] float2 fragTexCoord : TEXCOORD0;
};

[[vk::binding(0, 0)]]
Sampler2D texSampler;

struct SpritePushConstants {
    float4x4 mvp;
    float4 color;
    float4 uvRect;
};

[[vk::push_constant]]
SpritePushConstants push;

[shader("fragment")]
float4 main(PSInput input) : SV_Target {
    float2 uv = push.uvRect.xy + input.fragTexCoord * push.uvRect.zw;
    return texSampler.Sample(uv) * push.color;
}
