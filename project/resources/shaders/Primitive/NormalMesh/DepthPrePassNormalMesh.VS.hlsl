struct InstanceData
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4 color;
    uint32_t textureIndex;
    float32_t3 padding;
};

StructuredBuffer<InstanceData> gInstanceData : register(t0);

cbuffer CBPerBatch : register(b0)
{
    uint32_t gStartLocation;
};

struct VSInput
{
    float32_t4 position : POSITION;
    uint32_t instanceID : SV_InstanceID;
};

float32_t4 main(VSInput input) : SV_POSITION
{
    uint32_t index = gStartLocation + input.instanceID;
    
    float32_t4x4 wvp = gInstanceData[index].WVP;
    return mul(input.position, wvp);
}