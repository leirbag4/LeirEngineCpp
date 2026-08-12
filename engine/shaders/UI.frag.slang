struct PSInput {
    [[vk::location(0)]] float2 fragTexCoord : TEXCOORD0;
    [[vk::location(1)]] float4 fragColor : COLOR0;
};

[[vk::binding(0, 0)]]
Sampler2D texSampler;

[shader("fragment")]
float4 main(PSInput input) : SV_Target {
    float4 texColor = texSampler.Sample(input.fragTexCoord);
    return texColor * input.fragColor;
}
