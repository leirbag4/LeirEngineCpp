struct PSInput {
    [[vk::location(0)]] float3 fragNormal : TEXCOORD0;
    [[vk::location(1)]] float2 fragTexCoord : TEXCOORD1;
    [[vk::location(2)]] float3 fragWorldPos : TEXCOORD2;
};

[[vk::binding(0, 1)]]
Sampler2D texSampler;

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

[shader("fragment")]
float4 main(PSInput input) : SV_Target {
    float3 normal = normalize(input.fragNormal);
    float3 lightDir = normalize(push.lightDir);

    float diff = max(dot(normal, -lightDir), 0.0);
    float3 diffuse = push.lightColor * diff;

    float3 ambient = push.ambientColor;

    float4 texColor = texSampler.Sample(input.fragTexCoord);
    float4 baseColor = texColor * push.color;

    return float4((ambient + diffuse) * baseColor.rgb, baseColor.a);
}
