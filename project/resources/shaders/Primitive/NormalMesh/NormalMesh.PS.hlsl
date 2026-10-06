#include "NormalMesh.hlsli"

// 構造体
struct PixelShaderOutPut
{
    float32_t4 color : SV_TARGET0;
};

struct SDFInstanceGPU
{
    float32_t4x4 worldInverse;
    float32_t3 gridMin;
    uint32_t textureIndex;
    float32_t3 gridMax;
    float32_t padding;
};

cbuffer ShadowConfig : register(b0)
{
    float32_t3 gLightDir;
    uint32_t gActiveSDFCount;
}

// 宣言
//Texture2D<float32_t4> gTextures[] : register(t0);
StructuredBuffer<SDFInstanceGPU> gSDFInstances : register(t0);
SamplerState gSampler : register(s0);

// 光線がAABBと当たり判定を取る関数
bool RayAABBIntersection(float32_t3 rayOrigin, float32_t3 rayDir, float32_t3 boxMin, float32_t3 boxMax, out float32_t tNear, out float32_t tFar)
{
    // t0 と t1の計算で割り算に使うので先に計算
    float32_t3 invDir = 1.0f / rayDir;
    // rayの公式 P(t) = Origin + t * Dirを変形したもの
    // レイが AABB の最小面（gridMin の壁） に達するまでの距離
    float32_t3 t0 = (boxMin - rayOrigin) * invDir;
    // レイが AABB の最大面（gridMax の壁） に達するまでの距離
    float32_t3 t1 = (boxMax - rayOrigin) * invDir;

    // 進む方向で反転するための計算
    // 一番近い側の決定
    float32_t3 tMin = min(t0, t1);
    // 一番遠い点の決定
    float32_t3 tMax = max(t0, t1);

    // 完全に入った点の決定
    // 一番近い点(完全に入った点 -> 遅く入った点)を決定
    tNear = max(max(tMin.x, tMin.y), tMin.z);
    // 一番遠い点(出始めた点 -> 速く出た点)を決定
    tFar = min(min(tMax.x, tMax.y), tMax.z);

    // 入口より出口の方が後ろにある カツ 箱の出口がレイの進行方向の前方にある
    return (tNear <= tFar) && (tFar > 0.0f);
}

// worldPos : Rayの開始位置
// lightDir : Rayの向き
float32_t EvaluateSDFShadow(float32_t3 worldPos, float32_t3 lightDir)
{
    if (gActiveSDFCount == 0) return 1.0f;
    
    float32_t shadow = 1.0f;
    float32_t shadowSoftness = 16.0f;
    
    for (uint32_t i = 0; i < gActiveSDFCount; ++i)
    {
        SDFInstanceGPU inst = gSDFInstances[i];
        
        // 当たり判定を取るために二点しか戻さなくていい方をInverseで戻している
        float32_t3 localOrigin = mul(float32_t4(worldPos, 1.0f), inst.worldInverse).xyz;
        float32_t3 localDir = normalize(mul(float32_t4(lightDir, 0.0f), inst.worldInverse).xyz);

        // AABBでそもそも当たるかどうかを判定
        float32_t tNear, tFar;
        if (!RayAABBIntersection(localOrigin, localDir, inst.gridMin, inst.gridMax, tNear, tFar))
        {
            // 当たらなかったから
            continue;
        }
        
        float32_t selfShadowBias = 0.02f;

        // 自分自身を誤判定しないため
        //tNear = max(tNear, 0.0f);

        float32_t t = max(tNear, selfShadowBias);
        float32_t3 gridSize = inst.gridMax - inst.gridMin;
        
        // レイマーチングの処理(最大 回)
        for (int step = 0; step < 64; ++step)
        {
            // レイの位置を計算
            float32_t3 currentPos = localOrigin + localDir * t;
            // 3Dテクスチャの座標に合わせる
            float32_t3 uvw = (currentPos - inst.gridMin) / gridSize;
            
            // any : ベクトルの X, Y, Z のどれか1つでも条件を満たせば true
            // レイの位置が 3D テクスチャ（AABB）の範囲外に飛び出したか
            // レイが AABB の裏側（出口）から抜け出たか
            if (any(uvw < -0.01f) || any(uvw > 1.01f) || t >= tFar)
            {
                // レイが対象オブジェクトの領域外なので計算終了
                break;
            }
            
            float32_t3 clampedUVW = saturate(uvw);
            // ResourceDescriptorHeapなんてなくてしぬ
            Texture3D<float32_t> sdfTex = ResourceDescriptorHeap[inst.textureIndex];
            // SampleLevel is nani?
            float32_t dist = sdfTex.SampleLevel(gSampler, clampedUVW, 0);

            if (dist < 0.002f)
            {
                // 完全な影
                return 0.0f;
            }
            
            shadow = min(shadow, shadowSoftness * dist / t);
            t += max(dist, 0.0002f);
        }
    }
    
    // saturate : clamp
    return saturate(shadow);
}

PixelShaderOutPut main(VertexShaderOutput input)
{
    PixelShaderOutPut output;
    Texture2D<float32_t4> tex = ResourceDescriptorHeap[input.textureIndex];
    float32_t4 textureColor = tex.Sample(gSampler, input.texcoord);
    if (textureColor.a <= 0.0f)
    {
        discard;
    }
    
    float32_t3 normal = normalize(input.normal);

    // ベースカラー（固定色または input.color.rgb などをそのまま使用）
    float32_t3 baseColor = textureColor.rgb * input.color.rgb;

    // --- 環境光 (Ambient) ---
    float32_t3 ambientColor = float32_t3(0.15f, 0.18f, 0.25f);
    float32_t3 ambient = baseColor * ambientColor;

    // --- 直接光 (Direct Light / 太陽光) ---
    float32_t NdotL = saturate(dot(normal, gLightDir));
    float32_t3 lightColor = float32_t3(1.0f, 0.95f, 0.8f);

    float32_t3 diffuse = baseColor * lightColor * NdotL;

    // --- SDF 影の評価 ---
    float32_t shadowFactor = 1.0f;
    if (NdotL > 0.0f)
    {
        shadowFactor = EvaluateSDFShadow(input.worldPosition, gLightDir);
    }
    
    // --- 影を「直接光」のみに適用 ---
    float32_t3 directLight = diffuse * shadowFactor;

    // --- 最終色の合成 ---
    float32_t3 finalColor = ambient + directLight;
    
    output.color.rgb = finalColor;
    output.color.a = textureColor.a * input.color.a;
    
    return output;
}