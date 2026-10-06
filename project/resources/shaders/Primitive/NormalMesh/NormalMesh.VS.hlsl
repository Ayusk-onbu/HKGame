#include "NormalMesh.hlsli"
struct NormalMeshForGPU
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4 color;
    uint32_t textureIndex;
    float32_t3 padding;
};

StructuredBuffer<NormalMeshForGPU> gNormalMesh : register(t0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

cbuffer BatchInfo : register(b0)
{
    uint32_t gBaseInstaceIndex;
}

VertexShaderOutput main(VertexShaderInput input,uint32_t instancedId : SV_InstanceID)
{
    VertexShaderOutput output;
    // 現在のインスタンスデータを取得
    uint32_t index = instancedId + gBaseInstaceIndex;
    NormalMeshForGPU instData = gNormalMesh[index];
    
    // ローカル座標を取り出す
    float32_t4 localPos = input.position;
    output.position = mul(localPos, instData.WVP);
    
    output.worldPosition = mul(localPos, instData.World).xyz;
    
    output.texcoord = input.texcoord;
    output.color = instData.color;
    output.normal = normalize(mul(input.normal,(float32_t3x3) instData.World));
    output.textureIndex = instData.textureIndex;
    
    return output;
}