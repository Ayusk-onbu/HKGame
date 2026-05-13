#include "SkyBox.hlsli"

struct Material
{
    float32_t4 color;
};
ConstantBuffer<Material> gMaterial : register(b0);

struct PixelShaderOutPut
{
    float32_t4 color : SV_TARGET0;
};
TextureCube<float32_t4> gTexture : register(t0);

SamplerState gSampler : register(s0);

PixelShaderOutPut main(VertexShaderOutput input)
{
    PixelShaderOutPut output;
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    output.color = textureColor * gMaterial.color;
    return output;
}