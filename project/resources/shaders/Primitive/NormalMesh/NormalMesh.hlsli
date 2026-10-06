struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t4 color : COLOR0;
    nointerpolation uint32_t textureIndex : TEXCOORD1;
    float32_t3 worldPosition : TEXCOORD2;// Rayの開始位置
};