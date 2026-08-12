struct VSInput {
    [[vk::location(0)]] float3 inPosition : POSITION0;
    [[vk::location(1)]] float3 inNormal : NORMAL0;
    [[vk::location(2)]] float2 inTexCoord : TEXCOORD0;
};

struct VSOutput {
    [[vk::location(0)]] float3 fragNormal : TEXCOORD0;
    [[vk::location(1)]] float2 fragTexCoord : TEXCOORD1;
    [[vk::location(2)]] float3 fragWorldPos : TEXCOORD2;
    float4 position : SV_Position;
};

[[vk::binding(0, 0)]]
cbuffer UniformBufferObject {
    float4x4 viewProjection;
} ubo;

struct PushConstants {
    float3 lightDir;
    float pad0;
    float3 lightColor;
    float pad1;
    float3 ambientColor;
    float pad2;
    float4 color;
    float4x4 model;
};

[[vk::push_constant]]
PushConstants push;

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput output;
    float4 worldPos = mul(push.model, float4(input.inPosition, 1.0));
    output.position = mul(ubo.viewProjection, worldPos);
    output.fragNormal = mul((float3x3)push.model, input.inNormal);
    output.fragTexCoord = input.inTexCoord;
    output.fragWorldPos = worldPos.xyz;
    return output;
}
