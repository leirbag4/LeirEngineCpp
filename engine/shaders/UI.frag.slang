struct PSInput {
    [[vk::location(0)]] float2 fragTexCoord : TEXCOORD0;
    [[vk::location(1)]] float4 fragColor : COLOR0;
    [[vk::location(2)]] float fragTexIndex : TEXCOORD1;
};

// Bindless texture table (set 0). Batches merge quads that share a texture, so
// a single draw can contain several different texture indices (non-uniform) —
// hence NonUniformResourceIndex. The WEB (browser, naga) cannot compile
// binding_array, so LEIR_BINDLESS=0 degrades to a single texture bound per draw.
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

[shader("fragment")]
float4 main(PSInput input) : SV_Target {
    uint idx = (uint)input.fragTexIndex;
    // Non-uniform resource indexing (different texture indices within one
    // draw) requires an explicit annotation on SPIR-V/DXIL. On WGSL/Metal/GLSL
    // the non-uniformity is implicit, and NonUniformResourceIndex is not
    // available there — so it is only emitted for the targets that need it.
    uint nonUniformIdx;
    __target_switch {
    case hlsl:
    case spirv:
        nonUniformIdx = NonUniformResourceIndex(idx);
        break;
    default:
        nonUniformIdx = idx;
        break;
    }
    #if LEIR_BINDLESS
    float4 texColor = textures[nonUniformIdx].Sample(input.fragTexCoord);
#else
    float4 texColor = tex.Sample(input.fragTexCoord);
#endif
    return texColor * input.fragColor;
}