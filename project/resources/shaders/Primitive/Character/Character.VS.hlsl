#include "Character.hlsli"

// C++側の CharacterForGPU と一致させる
struct CharacterForGPU
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4x4 uvTransform;
    float32_t4 color;
    uint32_t colorTextureIndex;
    uint32_t normalTextureIndex;
    uint32_t skinMask1TextureIndex;
    uint32_t skinMask2TextureIndex;
    float32_t melaninScale;
    float32_t bloodVolumeScale;
    float32_t roughnessScale;
    float32_t sssScale;
};

StructuredBuffer<CharacterForGPU> gCharacter : register(t0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint32_t instancedId : SV_InstanceID)
{
    VertexShaderOutput output;
    CharacterForGPU instData = gCharacter[instancedId];
    
    output.position = mul(input.position, instData.WVP);
    output.texcoord = mul(float32_t4(input.texcoord, 0.0f, 1.0f), instData.uvTransform).xy;
    output.color = instData.color;
    
    // 法線をワールド空間に変換
    output.normal = normalize(mul(input.normal, (float32_t3x3) instData.World));
    
    
    // 頂点のワールド座標を計算してPSに送る
    output.worldPosition = mul(input.position, instData.World).xyz;
    
    output.colorTextureIndex = instData.colorTextureIndex;
    output.normalTextureIndex = instData.normalTextureIndex;
    output.skinMask1TextureIndex = instData.skinMask1TextureIndex;
    output.skinMask2TextureIndex = instData.skinMask2TextureIndex;
    output.melaninScale = instData.melaninScale;
    output.bloodVolumeScale = instData.bloodVolumeScale;
    output.roughnessScale = instData.roughnessScale;
    output.sssScale = instData.sssScale;
    return output;
}