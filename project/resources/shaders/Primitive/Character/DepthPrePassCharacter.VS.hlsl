
// インスタンスデータ
struct InstanceData
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

StructuredBuffer<InstanceData> gInstanceData : register(t0);

struct VSInput
{
    float32_t4 position : POSITION;
    uint32_t instanceID : SV_InstanceID;
};

float32_t4 main(VSInput input) : SV_POSITION
{
    float32_t4x4 wvp = gInstanceData[input.instanceID].WVP;
    
    // 座標変換して返却
    return mul(input.position,wvp);
}