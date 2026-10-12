// 入力テクスチャ（SRV）
Texture2D<float4> g_DirectLighting : register(t0); // RTV0
Texture2D<float4> g_SssRadiusMap : register(t1); // RTV1
Texture2D<float> g_DepthMap : register(t2); // DSV (深度バッファ)

// 出力テクスチャ（UAV: 横ブラーの中間結果）
RWTexture2D<float4> g_BlurOutput : register(u0);

// サンプラー
SamplerState g_LinearSampler : register(s0);

// SSS ガウシアンサンプリングカーネル（カーネル半径と重み）
// RGBで異なる重みを与えることで、赤が深く透ける物理特性を再現
static const int kNumSamples = 7;
static const float kOffsets[7] = { -3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0 };
static const float3 kWeights[7] =
{
    float3(0.006, 0.000, 0.000),
    float3(0.061, 0.002, 0.000),
    float3(0.242, 0.106, 0.012),
    float3(0.382, 0.784, 0.976), // 中心ピクセル
    float3(0.242, 0.106, 0.012),
    float3(0.061, 0.002, 0.000),
    float3(0.006, 0.000, 0.000)
};

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 currentCoord = dispatchThreadID.xy;

    // 現在のピクセルの情報を取得
    float4 centerColor = g_DirectLighting[currentCoord];
    float4 sssParam = g_SssRadiusMap[currentCoord];
    float centerDepth = g_DepthMap[currentCoord];

    // SSSが無効（散乱半径がほぼ0）または背景（深度上限）の場合はブラーをスキップ
    float3 sssRadius = sssParam.rgb;
    if (dot(sssRadius, 1.0) < 0.001 || centerDepth >= 1.0)
    {
        g_BlurOutput[currentCoord] = centerColor;
        return;
    }

    float3 accumulatedColor = float3(0.0, 0.0, 0.0);
    float3 accumulatedWeight = float3(0.0, 0.0, 0.0);

    // 横方向（X軸）に1Dタップ
    for (int i = 0; i < kNumSamples; ++i)
    {
        // 5.0の係数は散乱半径を画面ピクセル単位に変換するためのスケーリングファクター
        // 大きいと半透明に、小さいとプラスチックのようになる
        int2 sampleCoord = currentCoord + int2(kOffsets[i] * sssRadius.r * 5.0, 0); // 散乱半径を乗算

        float3 sampleColor = g_DirectLighting[sampleCoord].rgb;
        float sampleDepth = g_DepthMap[sampleCoord];

        // 深度差によるエッジ判定（輪郭の崩れを防ぐ）
        float depthDiff = abs(centerDepth - sampleDepth);
        // 1000.0の係数は深度差の感度を調整するためのスケーリングファクター
        float depthWeight = exp(-depthDiff * 1000.0); // 深度差が大きいと0に近くなる

        float3 weight = kWeights[i] * depthWeight;

        accumulatedColor += sampleColor * weight;
        accumulatedWeight += weight;
    }

    // 正規化して書き出し
    float3 finalColor = accumulatedColor / max(accumulatedWeight, float3(0.0001, 0.0001, 0.0001));
    g_BlurOutput[currentCoord] = float4(finalColor, centerColor.a);
}