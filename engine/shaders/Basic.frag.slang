struct PSInput {
    [[vk::location(0)]] float3 fragNormal : TEXCOORD0;
    [[vk::location(1)]] float2 fragTexCoord : TEXCOORD1;
    [[vk::location(2)]] float3 fragWorldPos : TEXCOORD2;
};

// Bindless texture table (set 1): the engine binds its global bindless set
// here and textures are referenced by index. The count is runtime (unbounded
// array); Vulkan/D3D12 back it with the backend's bindless table. The WEB
// (browser, naga) cannot compile binding_array, so LEIR_BINDLESS=0 degrades to
// a single texture that the WebGPU backend binds per draw.
#ifndef LEIR_BINDLESS
#define LEIR_BINDLESS 1
#endif

#if LEIR_BINDLESS
[[vk::binding(0, 1)]]
Sampler2D textures[];
#else
[[vk::binding(0, 1)]]
Sampler2D tex;
#endif

struct PushConstants {
    float3 lightDir;
    float pad0;
    float3 lightColor;
    float pad1;
    float3 ambientColor;
    float pad2;
    float4 color;
    float4x4 model;
    uint textureIndex;
};

[[vk::push_constant]]
PushConstants push : register(b1, space0);

[shader("fragment")]
float4 main(PSInput input) : SV_Target {
    float3 normal = normalize(input.fragNormal);
    float3 lightDir = normalize(push.lightDir);

    float diff = max(dot(normal, -lightDir), 0.0);
    float3 diffuse = push.lightColor * diff;

    float3 ambient = push.ambientColor;

    // The index comes from a push constant: uniform across the draw.
#if LEIR_BINDLESS
    float4 texColor = textures[push.textureIndex].Sample(input.fragTexCoord);
#else
    float4 texColor = tex.Sample(input.fragTexCoord);
#endif
    float4 baseColor = texColor * push.color;

    return float4((ambient + diffuse) * baseColor.rgb, baseColor.a);
}