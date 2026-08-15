struct VSInput {
    [[vk::location(0)]] float2 inPosition : POSITION0;
    [[vk::location(1)]] float2 inTexCoord : TEXCOORD0;
    [[vk::location(2)]] float4 inColor : COLOR0;
    // Per-vertex bindless texture index (batching preserves one draw per
    // texture; a quad's four vertices always carry the same index).
    [[vk::location(3)]] float inTexIndex : TEXCOORD1;
};

struct VSOutput {
    [[vk::location(0)]] float2 fragTexCoord : TEXCOORD0;
    [[vk::location(1)]] float4 fragColor : COLOR0;
    [[vk::location(2)]] float fragTexIndex : TEXCOORD1;
    float4 position : SV_Position;
};

[[vk::push_constant]]
float2 screenSize : register(b1, space0);

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
    output.fragTexIndex = input.inTexIndex;
    return output;
}