struct VSInput {
    [[vk::location(0)]] float2 inPosition : POSITION0;
    [[vk::location(1)]] float2 inTexCoord : TEXCOORD0;
    [[vk::location(2)]] float4 inColor : COLOR0;
};

struct VSOutput {
    [[vk::location(0)]] float2 fragTexCoord : TEXCOORD0;
    [[vk::location(1)]] float4 fragColor : COLOR0;
    float4 position : SV_Position;
};

[[vk::push_constant]]
float2 screenSize;

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput output;
    float2 ndc = float2(
        (input.inPosition.x / screenSize.x) * 2.0 - 1.0,
        (1.0 - input.inPosition.y / screenSize.y) * 2.0 - 1.0
    );
    output.position = float4(ndc, 0.0, 1.0);
    output.fragTexCoord = input.inTexCoord;
    output.fragColor = input.inColor;
    return output;
}
