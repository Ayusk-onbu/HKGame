struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t4 color : COLOR0;
    float32_t3 worldPosition : POSITION0; // TBNを作るために必要
    
    // 整数インデックス群（補間禁止）
    nointerpolation uint32_t colorTextureIndex : TEXCOORD1;
    nointerpolation uint32_t normalTextureIndex : TEXCOORD2;
    nointerpolation uint32_t skinMask1TextureIndex : TEXCOORD3;
    nointerpolation uint32_t skinMask2TextureIndex : TEXCOORD4;
    nointerpolation float32_t melaninScale : TEXCOORD5;
    nointerpolation float32_t bloodVolumeScale : TEXCOORD6;
    nointerpolation float32_t roughnessScale : TEXCOORD7;
    nointerpolation float32_t sssScale : TEXCOORD8;
};