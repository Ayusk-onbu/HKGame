#include "Fullscreen.hlsli"
#include "../SDFFunction.hlsli"

// --- 統合パラメータ構造体 ---
struct PostEffectConfig
{
    float32_t4x4 projectionInverse;
    float32_t3 cameraPos;
    uint32_t sdfInstanceCount;
    // 各エフェクトの有効化フラグ (0: OFF, 1: ON)
    int32_t enableVignette;
    int32_t enableRadialBlur;
    int32_t enableRandom;
    int32_t enableLuminanceOutline;
    int32_t enableGaussian;
    int32_t enableDepthOutline;
    int32_t enableBoxFilter;
    int32_t enableGrayscale;
    int32_t enableSDFDebug;
    
    // パラメータ類
    float32_t vignetteIntensity;
    float32_t radialBlurWidth;
    float32_t time; // Randomノイズ用
};

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t> gDepthTexture : register(t1);
StructuredBuffer<SDFFunctionInstanceGPU> gSDFInstances : register(t2);
StructuredBuffer<SDFFunctionStackNodeGPU> gSDFNodes : register(t3);
ConstantBuffer<PostEffectConfig> gConfig : register(b0);
SamplerState gSampler : register(s0);
SamplerState gSamplerPoint : register(s1);

float32_t3 ReconstructWorldPos(float32_t2 uv, float32_t depth)
{
    float32_t4 ndc = float32_t4(uv.x * 2.0f - 1.0f, (1.0f - uv.y) * 2.0f - 1.0f, depth, 1.0f);
    float32_t4 worldPos = mul(ndc, gConfig.projectionInverse);
    return worldPos.xyz / worldPos.w;
}

// 中心差分によるSDFの法線計算（立体的な陰影用）
float32_t3 CalcSDFNormal(SDFEvaluator sdf, float32_t3 p)
{
    const float32_t eps = 0.002f;
    int32_t dummy;
    float32_t dx = sdf.EvaluateScene(p + float32_t3(eps, 0, 0), gConfig.sdfInstanceCount, dummy)
                 - sdf.EvaluateScene(p - float32_t3(eps, 0, 0), gConfig.sdfInstanceCount, dummy);
    float32_t dy = sdf.EvaluateScene(p + float32_t3(0, eps, 0), gConfig.sdfInstanceCount, dummy)
                 - sdf.EvaluateScene(p - float32_t3(0, eps, 0), gConfig.sdfInstanceCount, dummy);
    float32_t dz = sdf.EvaluateScene(p + float32_t3(0, 0, eps), gConfig.sdfInstanceCount, dummy)
                 - sdf.EvaluateScene(p - float32_t3(0, 0, eps), gConfig.sdfInstanceCount, dummy);
    return normalize(float32_t3(dx, dy, dz));
}

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

static const float32_t2 kIndex3x3[3][3] =
{
    { { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f } },
    { { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } },
    { { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } }
};

static const float32_t2 kIndex5x5[5][5] =
{
    { { -2.0f, -2.0f }, { -1.0f, -2.0f }, { 0.0f, -2.0f }, { 1.0f, -2.0f }, { 2.0f, -2.0f } },
    { { -2.0f, -1.0f }, { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f }, { 2.0f, -1.0f } },
    { { -2.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 2.0f, 0.0f } },
    { { -2.0f, 1.0f }, { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 2.0f, 1.0f } },
    { { -2.0f, 2.0f }, { -1.0f, 2.0f }, { 0.0f, 2.0f }, { 1.0f, 2.0f }, { 2.0f, 2.0f } }
};

static const float32_t kKernel5x5[5][5] =
{
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f }
};

static const float32_t kPrewittHorizontalKernek[3][3] =
{
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f }
};

static const float32_t kPrewittVerticalKernel[3][3] =
{
    { -1.0f / 6.0f, -1.0f / 6.0f, -1.0f / 6.0f },
    { 0.0f, 0.0f, 0.0f },
    { 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f }
};

static const float32_t PI = 3.141592653589f;

float gausu(float x, float y, float sigma)
{
    float exponent = -(x * x + y * y) * rcp(2.0f * sigma * sigma);
    float denominator = 2.0f * PI * sigma * sigma;
    return exp(exponent) * rcp(denominator);
    // exp = e^exponent
}

float32_t Luminance(float32_t3 v)
{
    return dot(v, float32_t3(0.2125f, 0.7154f, 0.0721f));
}

float rand2dTo1d(float2 value, float2 dotDir = float2(12.9898, 78.233))
{
    float2 smallValue = sin(value);
    float random = dot(smallValue, dotDir);
    random = frac(sin(random) * 143758.5453);
    return random;
}

PixelShaderOutput main(VertexShaderOutput input)
{   
    PixelShaderOutput output;
    float32_t2 uv = input.texcoord;
    
    // 画面解像度の取得
    uint32_t width, height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = float32_t2(rcp(width), rcp(height));

    // 初期化：基本カラーのサンプリング
    output.color = gTexture.Sample(gSampler, uv);

    // -------------------------------------------------------------------
    // 1. Box Filter (5x5 平均化フィルタ)
    // -------------------------------------------------------------------
    if (gConfig.enableBoxFilter > 0)
    {
        float32_t3 boxColor = float32_t3(0.0f, 0.0f, 0.0f);
        for (int32_t x = 0; x < 5; ++x)
        {
            for (int32_t y = 0; y < 5; ++y)
            {
                float32_t2 texcoord = uv + kIndex5x5[x][y] * uvStepSize;
                boxColor += gTexture.Sample(gSampler, texcoord).rgb * kKernel5x5[x][y];
            }
        }
        output.color.rgb = boxColor;
    }

    // -------------------------------------------------------------------
    // 2. Gaussian Blur (ガウシアンフィルタ)
    // -------------------------------------------------------------------
    if (gConfig.enableGaussian > 0)
    {
        // 深度値を取得
        float32_t ndcDepth = gDepthTexture.Sample(gSamplerPoint, uv);
        
        // 逆行列でビュー空間座標に変換
        float32_t4 viewSpace = mul(float32_t4(0.0f, 0.0f, ndcDepth, 1.0f), gConfig.projectionInverse);
        
        float32_t viewZ = abs(viewSpace.z / viewSpace.w);

        // 距離設定 (ゲームのスケールに合わせて調整)
        // 手前 blurStart(単位) まんまはボカさず、blurEnd(単位) より奥を最大ぼかしにする
        float32_t blurStart = 5.0f;
        float32_t blurEnd = 30.0f;
        
        float32_t blurFactor = saturate((viewZ - blurStart) / (blurEnd - blurStart));

        if (blurFactor > 0.0f)
        {
            float32_t weight = 0.0f;
            float32_t3 blurColor = float32_t3(0.0f, 0.0f, 0.0f);

            // ぼかし幅の拡大倍率 (3.0f～5.0f 程度でクッキリボケます)
            float32_t blurScale = 4.0f;

            for (int32_t x = 0; x < 3; ++x)
            {
                for (int32_t y = 0; y < 3; ++y)
                {
                    float32_t kWeight = gausu(kIndex3x3[x][y].x, kIndex3x3[x][y].y, 2.0f);
                    float32_t2 texcoord = uv + kIndex3x3[x][y] * uvStepSize * blurScale;
                    blurColor += gTexture.Sample(gSampler, texcoord).rgb * kWeight;
                    weight += kWeight;
                }
            }
            float32_t3 finalBlur = blurColor * rcp(weight);

            // 手前はそのまま、奥だけぼかす
            output.color.rgb = lerp(output.color.rgb, finalBlur, blurFactor);
        }
    }

    // -------------------------------------------------------------------
    // 3. Radial Blur (放射状ぼかし)
    // -------------------------------------------------------------------
    if (gConfig.enableRadialBlur > 0)
    {
        const float32_t2 kCenter = float32_t2(0.5f, 0.5f);
        const int32_t kNumSamples = 10;
        
        float32_t2 direction = uv - kCenter;
        float32_t3 blurColor = float32_t3(0.0f, 0.0f, 0.0f);

        for (int32_t sampleIndex = 0; sampleIndex < kNumSamples; ++sampleIndex)
        {
            float32_t2 texcoord = uv + direction * gConfig.radialBlurWidth * float32_t(sampleIndex);
            blurColor += gTexture.Sample(gSampler, texcoord).rgb;
        }
    
        output.color.rgb = blurColor * rcp(kNumSamples);
    }

    // -------------------------------------------------------------------
    // 4. Luminance Based Outline (輝度ベース輪郭)
    // -------------------------------------------------------------------
    if (gConfig.enableLuminanceOutline > 0)
    {
        float32_t2 difference = float32_t2(0.0f, 0.0f);
        for (int32_t x = 0; x < 3; ++x)
        {
            for (int32_t y = 0; y < 3; ++y)
            {
                float32_t2 texcoord = uv + kIndex3x3[x][y] * uvStepSize;
                float32_t3 fetchColor = gTexture.Sample(gSampler, texcoord).rgb;
                float32_t luminance = Luminance(fetchColor);
            
                difference.x += luminance * kPrewittHorizontalKernek[x][y];
                difference.y += luminance * kPrewittVerticalKernel[x][y];
            }
        }
        float32_t weight = saturate(length(difference) * 6.0f);
        output.color.rgb *= (1.0f - weight);
    }

    // -------------------------------------------------------------------
    // 5. Depth Based Outline (深度ベース輪郭)
    // -------------------------------------------------------------------
    if (gConfig.enableDepthOutline > 0)
    {
        float32_t2 difference = float32_t2(0.0f, 0.0f);
        for (int32_t x = 0; x < 3; ++x)
        {
            for (int32_t y = 0; y < 3; ++y)
            {
                float32_t2 texcoord = uv + kIndex3x3[x][y] * uvStepSize;
                float32_t ndcDepth = gDepthTexture.Sample(gSamplerPoint, texcoord);
                float32_t4 viewSpace = mul(float32_t4(0.0f, 0.0f, ndcDepth, 1.0f), gConfig.projectionInverse);
                float32_t viewZ = viewSpace.z / rcp(viewSpace.w);
            
                difference.x += viewZ * kPrewittHorizontalKernek[x][y];
                difference.y += viewZ * kPrewittVerticalKernel[x][y];
            }
        }
        float32_t weight = saturate(length(difference));
        output.color.rgb *= (1.0f - weight);
    }

    // -------------------------------------------------------------------
    // 6. Grayscale (グレースケール)
    // -------------------------------------------------------------------
    if (gConfig.enableGrayscale > 0)
    {
        float32_t gray = Luminance(output.color.rgb);
        output.color.rgb = float32_t3(gray, gray, gray);
        
        // スキャンライン
        // 細いシマシマ（高速で下に流れる）
        float32_t fineLine = sin((uv.y - gConfig.time * 3.0f) * 800.0f);
        fineLine = saturate(fineLine * 0.5f + 0.5f);
        output.color.rgb *= lerp(0.85f, 1.0f, fineLine);

        // 太い走査バー（ゆっくり上から下に降りてくる）
        float32_t barPos = frac(gConfig.time * 0.2f); // 0～1を繰り返す
        float32_t bar = smoothstep(0.0f, 0.1f, 0.1f - abs(uv.y - barPos));
        
        // 通過する場所をちょっと明るく（ノイズ感アップ）
        output.color.rgb += float32_t3(bar, bar, bar) * 0.08f;
    }

    // -------------------------------------------------------------------
    // 7. Vignetting (画面端の減光)
    // -------------------------------------------------------------------
    if (gConfig.enableVignette > 0)
    {
        
        // 色収差
        // 画面中心からの距離に応じてズレを大きくする
        float32_t2 dir = uv - float32_t2(0.5f, 0.5f);
        float32_t2 offset = dir * 0.06;

        // 赤と青のチャンネルだけUVを左右反対にずらしてサンプリング
        float32_t r = gTexture.Sample(gSampler, uv + offset).r;
        float32_t g = output.color.g;
        float32_t b = gTexture.Sample(gSampler, uv - offset).b;

        output.color.rgb = float32_t3(r, g, b);
        
        float32_t2 correct = uv * (1.0f - uv.yx);
        float vignette = correct.x * correct.y * 16.0f;
        vignette = saturate(pow(vignette, 0.8f));
        
        float32_t factor = lerp(1.0f, vignette, gConfig.vignetteIntensity);
        output.color.gb *= factor;
    }

    // -------------------------------------------------------------------
    // 8. Random (ノイズ / グリッチ)
    // -------------------------------------------------------------------
    if (gConfig.enableRandom > 0)
    {
        float32_t random = rand2dTo1d(uv * gConfig.time);
        output.color.rgb *= random;
    }

    // -------------------------------------------------------------------
    // 9. SDF Debug Visualization (レイマーチング可視化)
    // -------------------------------------------------------------------
    if (gConfig.enableSDFDebug > 0 && gConfig.sdfInstanceCount > 0)
    {
        float32_t ndcDepth = gDepthTexture.Sample(gSamplerPoint, uv);
        float32_t3 sceneWorldPos = ReconstructWorldPos(uv, ndcDepth);
        float32_t maxRayDist = length(sceneWorldPos - gConfig.cameraPos);

        // Near と Far の2点からレイ方向を正確に算出
        float32_t3 nearWorldPos = ReconstructWorldPos(uv, 0.0f);
        float32_t3 farWorldPos = ReconstructWorldPos(uv, 1.0f);
        float32_t3 rayDir = normalize(farWorldPos - nearWorldPos);

        SDFEvaluator sdf;
        sdf.instances = gSDFInstances;
        sdf.nodes = gSDFNodes;

        float32_t t = 0.0f;
        const int32_t kMaxSteps = 128;
        const float32_t kHitThreshold = 0.005f;

        for (int32_t i = 0; i < kMaxSteps; ++i)
        {
            if (t >= maxRayDist)
                break;

            float32_t3 p = gConfig.cameraPos + rayDir * t;
            int32_t hitIdx = -1;
            float32_t d = sdf.EvaluateScene(p, gConfig.sdfInstanceCount, hitIdx);

            if (d < kHitThreshold)
            {
                float32_t3 normal = CalcSDFNormal(sdf, p);
                float32_t3 lightDir = normalize(float32_t3(0.5f, 1.0f, -0.5f));
                float32_t ndotl = dot(normal, lightDir) * 0.5f + 0.5f;
                float32_t3 debugColor = (normal * 0.5f + 0.5f) * ndotl;

                output.color.rgb = lerp(output.color.rgb, debugColor, 0.6f);
                break;
            }

            t += max(d, 0.01f);
        }
    }
    
    return output;
}