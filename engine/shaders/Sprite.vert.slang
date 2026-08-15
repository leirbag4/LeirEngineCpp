struct VSInput {
    [[vk::location(0)]] float2 inPosition : POSITION0;
    [[vk::location(1)]] float2 inTexCoord : TEXCOORD0;
};

struct VSOutput {
    [[vk::location(0)]] float2 fragTexCoord : TEXCOORD0;
    float4 position : SV_Position;
};

// textureIndex is consumed by the fragment stage; declared here so both stages
// produce the same push-constant layout (std430 size 112).
struct SpritePushConstants {
    float4x4 mvp;
    float4 color;
    float4 uvRect;
    uint textureIndex;
};

[[vk::push_constant]]
SpritePushConstants push : register(b1, space0);

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(push.mvp, float4(input.inPosition, 0.0, 1.0));
    output.fragTexCoord = input.inTexCoord;
    return output;
}