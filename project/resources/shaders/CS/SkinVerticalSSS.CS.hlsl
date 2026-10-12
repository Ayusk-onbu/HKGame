// 入力テクスチャ（SRV）
Texture2D<float4> g_HorizontalBlurOutput : register(t0); // 横ブラー結果
Texture2D<float4> g_SssRadiusMap : register(t1); // RTV1 (散乱半径マップ)
Texture2D<float> g_DepthMap : register(t2); // DSV (深度バッファ)

// 最終出力テクスチャ（UAV: バックバッファまたは最終画面ターゲット）
RWTexture2D<float4> g_FinalOutput : register(u0);

// SSS ガウシアンサンプリングカーネル (横パスと同一の重み構造)
static const int kNumSamples = 7;
static const float kOffsets[7] = { -3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0 };
static const float3 kWeights[7] =
{
    float3(0.006, 0.000, 0.000),
    float3(0.061, 0.002, 0.000),
    float3(0.242, 0.106, 0.012),
    float3(0.382, 0.784, 0.976),
    float3(0.242, 0.106, 0.012),
    float3(0.061, 0.002, 0.000),
    float3(0.006, 0.000, 0.000)
};

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 currentCoord = dispatchThreadID.xy;

    float4 centerColor = g_HorizontalBlurOutput[currentCoord];
    float4 sssParam = g_SssRadiusMap[currentCoord];
    float centerDepth = g_DepthMap[currentCoord];

    float3 sssRadius = sssParam.rgb;
    if (dot(sssRadius, 1.0) < 0.001 || centerDepth >= 1.0)
    {
        g_FinalOutput[currentCoord] = centerColor;
        return;
    }

    float3 accumulatedColor = float3(0.0, 0.0, 0.0);
    float3 accumulatedWeight = float3(0.0, 0.0, 0.0);

    // 縦方向（Y軸）に1Dタップ
    for (int i = 0; i < kNumSamples; ++i)
    {
        int2 sampleCoord = currentCoord + int2(0, kOffsets[i] * sssRadius.r * 5.0);

        float3 sampleColor = g_HorizontalBlurOutput[sampleCoord].rgb;
        float sampleDepth = g_DepthMap[sampleCoord];

        // 深度差によるエッジ判定（輪郭保護）
        float depthDiff = abs(centerDepth - sampleDepth);
        float depthWeight = exp(-depthDiff * 1000.0);

        float3 weight = kWeights[i] * depthWeight;

        accumulatedColor += sampleColor * weight;
        accumulatedWeight += weight;
    }

    float3 finalColor = accumulatedColor / max(accumulatedWeight, float3(0.0001, 0.0001, 0.0001));
    
    // 最終画面バッファへ書き出し
    g_FinalOutput[currentCoord] = float4(finalColor, centerColor.a);
}