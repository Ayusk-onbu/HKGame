#include "Hair.hlsli"

///////////////
//
// RayGenShader
//
///////////////

// Hit Information Store <- What
struct RayPayload
{
    float32_t3 color;
};

// Hit Attribute Store <- Where Hit
struct HairAttribute
{
    float32_t2 uv; // x: 中心軸からの距離比率(0~1), y: 節の始点からの位置比率(0~1)
    float32_t3 normal; // ワールド空間における法線
};

struct Camera
{
    float32_t4x4 inverseViewProj;
    float32_t3 position;
};

struct HairLightingParams
{
    float32_t3 lightDirection; // ライトの方向 (12バイト)
    float32_t pad0; // 16バイトの境界に合わせるためのパディング (4バイト)
    
    float32_t3 lightColor; // ライトの色 (12バイト)
    float32_t lightIntensity; // ライトの強さ (4バイト)
    
    float32_t3 ambientColor; // 環境光の色 (12バイト)
    float32_t hairShift; // キューティクルの傾き (4バイト)
    
    float32_t hairExponent; // 天使の輪の鋭さ (4バイト)
    float32_t3 pad1; // 16バイトの境界に合わせるためのパディング (12バイト)
};

// TLAS
RaytracingAccelerationStructure SceneTLAS : register(t0);
// Hair Vertex Buffer
//StructuredBuffer<StrandVertex> HairVertices : register(t1);
StructuredBuffer<StrandVertex> HairFlatVertices : register(t1);
// シーンのDepth情報
Texture2D<float32_t> SceneDepth : register(t2);
// Segment Data Buffer
//StructuredBuffer<SegmentData> HairSegments : register(t3);

// Camera Buffer
ConstantBuffer<Camera> gCamera : register(b0);

ConstantBuffer<HairLightingParams> g_LightingParams : register(b1);
// UAV
RWTexture2D<float32_t4> gOutput : register(u0);


[shader("raygeneration")]
void MyRayGenShader()
{
  // ----------------------------------------
    
    // Get Pixel Coord and Dimension
    uint32_t2 pixelCoord = DispatchRaysIndex().xy;
    uint32_t2 resolution = DispatchRaysDimensions().xy;
    
    float32_t4 originalColor = gOutput[pixelCoord];
    
    float32_t depth = SceneDepth.Load(int32_t3(pixelCoord, 0)); // 0.0(手前) ～ 1.0(一番奥)
    
    // Transform to NDC -> (0 - 1) -> (0 - 2) -> (-1 - 1)
    float32_t2 ndc = (float32_t2(pixelCoord)) / float32_t2(resolution) * 2.0f - 1.0f;
    // Flip
    ndc.y = -ndc.y;
    
  // ----------------------------------------
    
    // Get World Position
    float32_t4 target = mul(float32_t4(ndc.x, ndc.y, 1.0f, 1.0f), gCamera.inverseViewProj);
    // Perspective Divide
    float32_t3 worldTarget = target.xyz / target.w;

  // ----------------------------------------
    
    // Get Ray Direction (gCamera -> target Vector)
    float32_t3 rayDir = normalize(worldTarget - gCamera.position);

  // ----------------------------------------
    
    float32_t2 uv = ((float32_t2) pixelCoord + 0.5f) / (float32_t2) resolution;
    
    // デプス値(Z)と画面のUV(X,Y)から、不透明オブジェクト（顔など）のワールド座標を復元する
    float32_t4 clipPos;
    clipPos.x = uv.x * 2.0f - 1.0f;
    clipPos.y = (1.0f - uv.y) * 2.0f - 1.0f; // Y軸の上下反転
    clipPos.z = depth; // 読み取った深度値
    clipPos.w = 1.0f;

    // カメラの逆行列を掛けてクリップ空間 ➔ ワールド空間へ
    float32_t4 worldPos = mul(clipPos, gCamera.inverseViewProj);
    worldPos.xyz /= worldPos.w; // w除算で正確なワールド座標の完成

    // カメラから不透明オブジェクトまでの距離を計算する
    float32_t3 viewVec = worldPos.xyz - gCamera.position;
    float32_t maxDistance = length(viewVec); // これがレイを飛ばしていい限界の長さ！
    
  // ----------------------------------------
    
    // Setup Ray
    RayDesc ray;
    ray.Origin = gCamera.position;
    ray.Direction = rayDir;
    ray.TMin = 0.001f; // Collision Distance Min
    //ray.TMax = 10000.0f;// Collision Distance Max
    ray.TMax = maxDistance; // Collision Distance Max
    
  // ----------------------------------------
    
    // Setup Payload
    RayPayload payload = { originalColor.xyz };
    //RayPayload payload = { float32_t3(0.0f, 0.0f, 1.0f) };
  
  // ----------------------------------------
    
    // Ray Trace
    TraceRay(SceneTLAS, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 1, 0, ray, payload);

  // ----------------------------------------
    
    // Drawing
    gOutput[pixelCoord] = float32_t4(payload.color, 1.0f);
}

////////////////////////
//
//  Intersection Shader
//
////////////////////////

struct Ray
{
    float3 Origin;
    float3 Direction;
};

bool RayCapsuleIntersectionTest(Ray ray, float3 p0, float3 p1, float radius, float rayTCurrent, out float thit, out HairAttribute attr)
{
    thit = 0.0f;
    attr = (HairAttribute) 0;

    // 線分の方向と長さを計算
    float3 delta = ray.Origin - p0;
    float3 v = p1 - p0;
    float L = length(v);
    if (L < 0.0001f)
        return false; // 点潰れはスキップ
    float3 W = v / L; // 線分の単位方向ベクトル

    // 2直線の関係を解くための準備
    float dotDW = dot(ray.Direction, W);
    float denom = 1.0f - dotDW * dotDW; // 平行判定用の分母

    float t = 0.0f; // レイ上の距離
    float u = 0.0f; // 線分上の距離

    if (denom < 0.0001f)
    {
        // レイと線分が完全に平行な場合の処理
        t = -dot(delta, ray.Direction);
        u = t * dotDW + dot(delta, W);
    }
    else
    {
        // 通常のねじれの位置にある場合の数式解法
        float dotDD = dot(delta, ray.Direction);
        float dotDW_delta = dot(delta, W);

        t = (dotDW_delta * dotDW - dotDD) / denom;
        u = t * dotDW + dotDW_delta;
    }

    // クンプ前の大雑把な t の時点で、すでに現在の最短ヒット距離(rayTCurrent)より
    // 遥か後ろ（奥）にあることが分かったら、この先の重いクランプやベクトル計算をせずに即座に捨てる！
    // ※カプセルの半径分(radius)だけ安全マージンを持たせます
    if (t > rayTCurrent + radius)
    {
        return false;
    }
    
    // 無限の線を「有限の長さ」にクリッピング(クランプ)する
    float uClamped = clamp(u, 0.0f, L);

    // 範囲外に飛び出ていたら、丸め込んだ端点を使ってレイ上の衝突位置 t を再計算
    if (uClamped != u)
    {
        u = uClamped;
        t = -dot(delta - u * W, ray.Direction);
        
        // 端点に補正したあとの t でも再度チェック。超えていたら捨てる。
        if (t > rayTCurrent + radius)
        {
            return false;
        }
    }

    // レイ上の最接近点と、線分上の最接近点のリアルな3D座標を計算
    float3 hitPointRay = ray.Origin + t * ray.Direction;
    float3 hitPointSegment = p0 + u * W;
    
    // 中心軸からのズレ（距離）ベクトル
    float3 distVec = hitPointRay - hitPointSegment;
    float distSq = dot(distVec, distVec);

    // 髪の毛の太さ（半径）の内側を通過しているか判定
    if (distSq <= radius * radius)
    {
        thit = t; // 衝突距離を確定
        
        // ClosestHitへ渡す属性を公式の手順に沿って計算
        attr.uv.x = sqrt(distSq) / radius; // 太さに対する当たった位置の比率
        attr.uv.y = u / L; // 線分の長さに対する当たった位置の比率 (0.0〜1.0)
        
        // ローカル空間での法線（芯から外側へ向かうベクトル）
        float distLengthSq = dot(distVec, distVec);
        if (distLengthSq > 0.000001f)
        {
            attr.normal = distVec / sqrt(distLengthSq); // normalizeと同等
        }
        else
        {
    // 中心を貫いた場合は、カメラ方向に向いた適当な法線を返す
            attr.normal = -ray.Direction;
        }
        
        return true;
    }

    return false;
}

[shader("intersection")]
void HairIntersectionShader()
{
    float rayTCurrent = RayTCurrent();
    float rayTMin = RayTMin();
    
    // 現在処理中のレイ情報を取得
    Ray ray;
    ray.Origin = ObjectRayOrigin();
    ray.Direction = ObjectRayDirection();

    // レイトレーシングエンジンが教えてくれる「当たったAABBの通し番号」
    uint aabbIndex = PrimitiveIndex();
    StrandVertex v0 = HairFlatVertices[aabbIndex * 2 + 0];
    StrandVertex v1 = HairFlatVertices[aabbIndex * 2 + 1];
    
    float radius = v0.radius;

    // レイの始点から、線分の両端点（v0, v1）への距離の最小値を大雑把にチェック。
    // 完全に rayTCurrent より奥にある AABB なら、交差テストすら行わずに終了する。
    // (AABBをかすめただけの無駄な奥のレイをここで9割以上ふるい落とせます)
    float3 toV0 = v0.position - ray.Origin;
    float3 toV1 = v1.position - ray.Origin;
    float minDistEstimate = min(dot(toV0, ray.Direction), dot(toV1, ray.Direction)) - radius;
    if (minDistEstimate > rayTCurrent)
    {
        return;
    }
    
    float thit;
    HairAttribute attr;

    // 数学テストを実行：ここに改良の余地あり
    if (RayCapsuleIntersectionTest(ray, v0.position, v1.position, radius, rayTCurrent, thit, attr))
    {
        // 計算された交点 t が、現在のレイトレーシングの有効範囲内（一番手前）にあるか確認
        if (thit > rayTMin && thit < rayTCurrent)
        {
            // ローカル空間で計算された法線を、世界の傾き（ワールド空間）に変換する
            // ※髪の毛のデータがBLASの時点で配置されている場合は ObjectToWorld で一発で変換
            attr.normal = normalize(mul((float3x3) ObjectToWorld3x4(), attr.normal));

            // GPUへ交差を報告！（これによりClosestHitが起動する）
            ReportHit(thit, 0, attr);
        }
    }
}

// =================================================================
// 髪の毛の最近接ヒットシェーダー (Kajiya-Kayモデル版)
// =================================================================

float3 ShapeHairR_Dual(float rawR, float3 lightColor)
{
    // ① 周辺の裾野用：広くて柔らかいツヤ (範囲: 0.0 ~ 0.6)
    //    pow(rawR, 1.5) で少し広げてから 0.6 を掛ける
    float baseScatter = pow(saturate(rawR), 1.5f) * 0.6f;

    // ② 中心のハイライトコア用：鋭く強いツヤ (範囲: 0.0 ~ 8.0)
    //    pow(rawR, 16.0) で鋭く絞ってから 8.0 を掛ける
    float coreHighlight = pow(saturate(rawR), 16.0f) * 8.0f;

    // ③ 両方を合算！ 
    //    ・ハイライトがない場所 ＝ 0.0
    //    ・ハイライトの周辺     ＝ 0.0 ~ 0.6
    //    ・ハイライトの中心     ＝ 0.6 + 8.4 ≒ 1.0 ~ 9.0 (HDR輝度)
    float finalIntensity = baseScatter + coreHighlight;

    return lightColor * finalIntensity;
}

// =================================================================
// Marschner Model: R-Path (Primary Specular)
// =================================================================
float3 EvaluateHair_RPath(
    float3 L, // 光源への方向 (World Space, Normalized)
    float3 V, // カメラへの方向 (World Space, Normalized)
    float3 T, // 髪の接線ベクトル (World Space, Normalized)
    float roughness, // 表面粗さ Beta (0.15 ~ 0.25 推奨)
    float cuticleTilt, // キューティクルの傾き Alpha (例: -0.035 rad ≒ -2度)
    float3 lightColor // ライトの色・強度
)
{
    // -------------------------------------------------------------
    // 1. 縦方向の角度計算 (Theta)
    // -------------------------------------------------------------
    float sinThetaI = clamp(dot(L, T), -0.999f, 0.999f);
    float sinThetaR = clamp(dot(V, T), -0.999f, 0.999f);
    float cosThetaI = sqrt(1.0f - sinThetaI * sinThetaI);
    float cosThetaR = sqrt(1.0f - sinThetaR * sinThetaR);

    // -------------------------------------------------------------
    // 2. 縦方向の差分角 (theta_d)
    // -------------------------------------------------------------
    float cosThetaD = sqrt(max(0.001f, 0.5f * (1.0f + cosThetaI * cosThetaR + sinThetaI * sinThetaR)));
    float invCos2ThetaD = 1.0f / (cosThetaD * cosThetaD);

    // -------------------------------------------------------------
    // 3. 横方向の方位角 (Phi)
    // -------------------------------------------------------------
    float3 L_proj = L - sinThetaI * T;
    float3 V_proj = V - sinThetaR * T;
    float lenL = length(L_proj);
    float lenV = length(V_proj);

    float cosHalfPhi = 1.0f;
    if (lenL > 0.001f && lenV > 0.001f)
    {
        float3 L_perp = L_proj / lenL;
        float3 V_perp = V_proj / lenV;
        float cosPhi = clamp(dot(L_perp, V_perp), -1.0f, 1.0f);
        cosHalfPhi = sqrt(max(0.0f, 0.5f * (1.0f + cosPhi)));
    }

    // -------------------------------------------------------------
    // 4. 縦方向散乱 Mp (ガウス分布)
    // -------------------------------------------------------------
    float betaR = max(0.05f, roughness);
    float shift = sinThetaI + sinThetaR - cuticleTilt;
    float Mp = (1.0f / (betaR * 2.50662827f)) * exp(-(shift * shift) / (2.0f * betaR * betaR));

    // -------------------------------------------------------------
    // 5. 横方向散乱 Np & Schlick フレネル
    // -------------------------------------------------------------
    float F0 = 0.046f; // 髪の屈折率 n ≈ 1.55
    float Fresnel = F0 + (1.0f - F0) * pow(clamp(1.0f - cosThetaD, 0.0f, 1.0f), 5.0f);
    float Np = cosHalfPhi * Fresnel;

    // -------------------------------------------------------------
    // 6. 最終 BSDF 出力 (BSDF * cosThetaI)
    // -------------------------------------------------------------
    //float specIntensity = g_LightingParams.lightIntensity;
    float specIntensity = g_LightingParams.lightIntensity;
    float3 S_R = lightColor * (Mp * Np * invCos2ThetaD * cosThetaI) * specIntensity;
    
    return S_R;
}

// -------------------------------------------------------------
// Epic Games (UE4/UE5) Hair TT-Path (透過光) 高速実装
// -------------------------------------------------------------
float3 EvaluateHair_TTPath(
    float3 L, // ライト方向 (表面 -> 光源)
    float3 V, // カメラ方向 (表面 -> カメラ)
    float3 T, // 髪の接線 (毛根 -> 毛先)
    float roughness, // 粗さ (0.0 ~ 1.0)
    float3 baseColor, // 髪のベースカラー (RGB)
    float lightIntensity, // ライト強度
    float3 lightColor // ライトカラー
)
{
    // -------------------------------------------------------------
    // 1. 縦方向の角度計算 (Sin / Cos)
    // -------------------------------------------------------------
    float sinThetaI = clamp(dot(L, T), -0.999f, 0.999f);
    float sinThetaR = clamp(dot(V, T), -0.999f, 0.999f);
    float cosThetaI = sqrt(max(0.0f, 1.0f - sinThetaI * sinThetaI));
    float cosThetaR = sqrt(max(0.0f, 1.0f - sinThetaR * sinThetaR));

    // 差分角 cos(theta_d)
    float cosThetaD = sqrt(max(0.001f, 0.5f * (1.0f + cosThetaI * cosThetaR + sinThetaI * sinThetaR)));
    float invCos2ThetaD = 1.0f / (cosThetaD * cosThetaD);

    // -------------------------------------------------------------
    // 2. M_TT : 縦方向散乱 (ガウス分布)
    // TTパスは Rパスより広がり(粗さ)が半減し、シフト角も逆向きになる
    // -------------------------------------------------------------
    float betaR = max(0.05f, roughness * roughness);
    float betaTT = betaR * 0.5f; // TT用の粗さ (Rの半分)
    
    float alphaR = -0.06f; // Rパスのシフト角 (約 -3.5度)
    float alphaTT = -alphaR * 0.5f; // TTパスのシフト角 (+1.75度)

    float shiftTT = sinThetaI + sinThetaR - alphaTT;
    float Mp_TT = (1.0f / (betaTT * 2.50662827f)) * exp(-(shiftTT * shiftTT) / (2.0f * betaTT * betaTT));

    // -------------------------------------------------------------
    // 3. cosPhi & cosHalfPhi の計算 (逆三角関数なし)
    // -------------------------------------------------------------
    float3 L_proj = L - sinThetaI * T;
    float3 V_proj = V - sinThetaR * T;
    float lenL = length(L_proj);
    float lenV = length(V_proj);

    float cosPhi = 1.0f;
    float cosHalfPhi = 1.0f;

    if (lenL > 0.001f && lenV > 0.001f)
    {
        cosPhi = clamp(dot(L_proj / lenL, V_proj / lenV), -1.0f, 1.0f);
        cosHalfPhi = sqrt(max(0.0f, 0.5f * (1.0f + cosPhi)));
    }

    // -------------------------------------------------------------
    // 4. 修正屈折率 n' & a (Slide 26)
    // -------------------------------------------------------------
    float etaPrime = (1.19f / cosThetaD) + (0.36f * cosThetaD);
    float a = 1.0f / etaPrime;

    // -------------------------------------------------------------
    // 5. オフセット h_TT の計算 (Slide 25)
    // -------------------------------------------------------------
    float h_TT = (1.0f + a * (0.6f - 0.8f * cosPhi)) * cosHalfPhi;

    // -------------------------------------------------------------
    // 6. 吸収項 Absorption (Slide 28)
    // -------------------------------------------------------------
    float sqrtTerm = sqrt(max(0.0f, 1.0f - h_TT * h_TT * a * a));
    
    // BaseColor (C) による着色
    float3 safeColor = max(float3(0.001f, 0.001f, 0.001f), baseColor);
    float3 Absorption = pow(safeColor, float3(sqrtTerm / cosThetaD, sqrtTerm / cosThetaD, sqrtTerm / cosThetaD));

    // -------------------------------------------------------------
    // 7. 横方向散乱 D_TT (Slide 29)
    // -------------------------------------------------------------
    float D_TT = exp(-3.65f * cosPhi - 3.98f);

    // -------------------------------------------------------------
    // 8. フレネル透過率 F_TT (Slide 20, 23)
    // 表面と裏面の2回透過するので (1 - F)^2
    // -------------------------------------------------------------
    float F0 = 0.046f;
    float Fresnel = F0 + (1.0f - F0) * pow(saturate(1.0f - cosThetaD), 5.0f);
    float F_TT = (1.0f - Fresnel) * (1.0f - Fresnel);

    // -------------------------------------------------------------
    // 9. 最終合成 (S_TT)
    // -------------------------------------------------------------
    float3 Np_TT = D_TT * F_TT * Absorption;
    float3 S_TT = lightColor * (Mp_TT * Np_TT * invCos2ThetaD * cosThetaI) * lightIntensity;

    return S_TT;
}

// -------------------------------------------------------------
// Epic Games (UE4/UE5) Hair TRT-Path (二次内部反射光) 高速実装
// -------------------------------------------------------------
float3 EvaluateHair_TRTPath(
    float3 L, // ライト方向 (表面 -> 光源)
    float3 V, // カメラ方向 (表面 -> カメラ)
    float3 T, // 髪の接線 (毛根 -> 毛先)
    float roughness, // 粗さ (0.0 ~ 1.0)
    float3 baseColor, // 髪のベースカラー (RGB)
    float lightIntensity, // ライト強度
    float3 lightColor // ライトカラー
)
{
    // -------------------------------------------------------------
    // 1. 縦方向の角度計算 (Sin / Cos)
    // -------------------------------------------------------------
    float sinThetaI = clamp(dot(L, T), -0.999f, 0.999f);
    float sinThetaR = clamp(dot(V, T), -0.999f, 0.999f);
    float cosThetaI = sqrt(max(0.0f, 1.0f - sinThetaI * sinThetaI));
    float cosThetaR = sqrt(max(0.0f, 1.0f - sinThetaR * sinThetaR));

    // 差分角 cos(theta_d)
    float cosThetaD = sqrt(max(0.001f, 0.5f * (1.0f + cosThetaI * cosThetaR + sinThetaI * sinThetaR)));
    float invCos2ThetaD = 1.0f / (cosThetaD * cosThetaD);

    // -------------------------------------------------------------
    // 2. M_TRT : 縦方向散乱 (ガウス分布)
    // TRTは粗さが2倍に広がり、シフト角も逆方向(根元側)にずれる
    // -------------------------------------------------------------
    float betaR = max(0.05f, roughness * roughness);
    float betaTRT = betaR * 2.0f; // TRT用の粗さ (Rの2倍)

    float alphaR = -0.06f; // Rパスのシフト角 (毛先方向約 -3.5度)
    float alphaTRT = -3.0f * alphaR; // TRTパスのシフト角 (根元方向約 +10.5度)

    float shiftTRT = sinThetaI + sinThetaR - alphaTRT;
    float Mp_TRT = (1.0f / (betaTRT * 2.50662827f)) * exp(-(shiftTRT * shiftTRT) / (2.0f * betaTRT * betaTRT));

    // -------------------------------------------------------------
    // 3. cosPhi の計算 (逆三角関数なし)
    // -------------------------------------------------------------
    float3 L_proj = L - sinThetaI * T;
    float3 V_proj = V - sinThetaR * T;
    float lenL = length(L_proj);
    float lenV = length(V_proj);

    float cosPhi = 1.0f;
    if (lenL > 0.001f && lenV > 0.001f)
    {
        cosPhi = clamp(dot(L_proj / lenL, V_proj / lenV), -1.0f, 1.0f);
    }

    // -------------------------------------------------------------
    // 4. 横方向散乱 D_TRT (Slide 32)
    // -------------------------------------------------------------
    float D_TRT = exp(17.0f * cosPhi - 16.78f);

    // -------------------------------------------------------------
    // 5. 吸収項 Absorption_TRT (Slide 32)
    // 内部を2回往復通過するため BaseColor (C) で色づく
    // -------------------------------------------------------------
    float3 safeColor = max(float3(0.001f, 0.001f, 0.001f), baseColor);
    float expTerm = 0.8f / cosThetaD;
    float3 Absorption_TRT = pow(safeColor, float3(expTerm, expTerm, expTerm));

    // -------------------------------------------------------------
    // 6. フレネル & 減衰 F_TRT (h = sqrt(3)/2 固定)
    // 屈折2回 + 内部反射1回: F * (1 - F)^2
    // -------------------------------------------------------------
    float F0 = 0.046f;
    float Fresnel = F0 + (1.0f - F0) * pow(saturate(1.0f - cosThetaD), 5.0f);
    float F_TRT = Fresnel * (1.0f - Fresnel) * (1.0f - Fresnel);

    // -------------------------------------------------------------
    // 7. 最終 TRT 出力 (S_TRT)
    // -------------------------------------------------------------
    float3 Np_TRT = D_TRT * F_TRT * Absorption_TRT;
    float3 S_TRT = lightColor * (Mp_TRT * Np_TRT * invCos2ThetaD * cosThetaI) * lightIntensity;

    return S_TRT;
}

// -------------------------------------------------------------
// Marschner / Epic Games 完全統合ライティング関数
// -------------------------------------------------------------
float3 EvaluateHair_Epic_Unified(
    float3 L, // 光源方向 (表面 -> ライト)
    float3 V, // カメラ方向 (表面 -> カメラ)
    float3 T, // 髪の接線 (毛根 -> 毛先)
    float3 baseColor, // C : ベースカラー
    float roughness, // Roughness : 0.0 ~ 1.0
    float shift, // Shift : キューティクルの傾き (例: 0.03 rad ≒ 約1.7度)
    float specular, // Specular : R パスの強度スケール (通常 1.0)
    float scatter, // Scatter : 多重散乱の強度スケール (通常 1.0)
    float lightIntensity, // ライト強度
    float3 lightColor // ライトカラー
)
{
    // =========================================================
    // 1. アーティストパラメータの内部マッピング (Slide 44)
    // =========================================================
    // 粗さ (Beta) の変換: Beta_R = Roughness^2
    float betaR = max(0.001f, roughness * roughness);
    float betaTT = 0.5f * betaR; // Beta_TT  = 0.5 * Roughness^2
    float betaTRT = 2.0f * betaR; // Beta_TRT = 2.0 * Roughness^2

    // シフト角 (Alpha) の変換
    float alphaR = -2.0f * shift; // Alpha_R   = -2 * Shift
    float alphaTT = 1.0f * shift; // Alpha_TT  =  1 * Shift
    float alphaTRT = 4.0f * shift; // Alpha_TRT =  4 * Shift

    // =========================================================
    // 2. 共通の角度・ベクトル計算
    // =========================================================
    float sinThetaI = clamp(dot(L, T), -0.999f, 0.999f);
    float sinThetaR = clamp(dot(V, T), -0.999f, 0.999f);
    float cosThetaI = sqrt(max(0.0f, 1.0f - sinThetaI * sinThetaI));
    float cosThetaR = sqrt(max(0.0f, 1.0f - sinThetaR * sinThetaR));

    // 縦方向の差分角 (theta_d)
    float cosThetaD = sqrt(max(0.001f, 0.5f * (1.0f + cosThetaI * cosThetaR + sinThetaI * sinThetaR)));
    float invCos2ThetaD = 1.0f / (cosThetaD * cosThetaD);

    // 横方向の方位角 (Phi)
    float3 L_proj = L - sinThetaI * T;
    float3 V_proj = V - sinThetaR * T;
    float lenL = length(L_proj);
    float lenV = length(V_proj);

    float cosPhi = 1.0f;
    float cosHalfPhi = 1.0f;
    if (lenL > 0.001f && lenV > 0.001f)
    {
        cosPhi = clamp(dot(L_proj / lenL, V_proj / lenV), -1.0f, 1.0f);
        cosHalfPhi = sqrt(max(0.0f, 0.5f * (1.0f + cosPhi)));
    }

    // フレネル (屈折率 n ≈ 1.55, F0 ≈ 0.046)
    float F0 = 0.046f;
    float Fresnel = F0 + (1.0f - F0) * pow(saturate(1.0f - cosThetaD), 5.0f);
    float3 safeColor = max(float3(0.001f, 0.001f, 0.001f), baseColor);

    // =========================================================
    // 3. R パス (一次表面反射・白ツヤ)
    // =========================================================
    float shiftR = sinThetaI + sinThetaR - alphaR;
    float Mp_R = (1.0f / (betaR * 2.50662827f)) * exp(-(shiftR * shiftR) / (2.0f * betaR * betaR));
    float Np_R = cosHalfPhi;
    // Specular パラメータでスケール
    float3 S_R = lightColor * (Mp_R * Np_R * Fresnel * invCos2ThetaD * cosThetaI) * specular;

    // =========================================================
    // 4. TT パス (透過光)
    // =========================================================
    float shiftTT = sinThetaI + sinThetaR - alphaTT;
    float Mp_TT = (1.0f / (betaTT * 2.50662827f)) * exp(-(shiftTT * shiftTT) / (2.0f * betaTT * betaTT));

    float etaPrime = (1.19f / cosThetaD) + (0.36f * cosThetaD);
    float a = 1.0f / etaPrime;
    float h_TT = (1.0f + a * (0.6f - 0.8f * cosPhi)) * cosHalfPhi;
    
    float sqrtTerm = sqrt(max(0.0f, 1.0f - h_TT * h_TT * a * a));
    float3 Abs_TT = pow(safeColor, float3(sqrtTerm / cosThetaD, sqrtTerm / cosThetaD, sqrtTerm / cosThetaD));
    float D_TT = exp(-3.65f * cosPhi - 3.98f);
    float F_TT = (1.0f - Fresnel) * (1.0f - Fresnel);

    float3 Np_TT = D_TT * F_TT * Abs_TT;
    float3 S_TT = lightColor * (Mp_TT * Np_TT * invCos2ThetaD * cosThetaI);

    // =========================================================
    // 5. TRT パス (二次内部反射・色付きツヤ)
    // =========================================================
    float shiftTRT = sinThetaI + sinThetaR - alphaTRT;
    float Mp_TRT = (1.0f / (betaTRT * 2.50662827f)) * exp(-(shiftTRT * shiftTRT) / (2.0f * betaTRT * betaTRT));

    float D_TRT = exp(17.0f * cosPhi - 16.78f);
    float3 Abs_TRT = pow(safeColor, float3(0.8f / cosThetaD, 0.8f / cosThetaD, 0.8f / cosThetaD));
    float F_TRT = Fresnel * (1.0f - Fresnel) * (1.0f - Fresnel);

    float3 Np_TRT = D_TRT * F_TRT * Abs_TRT;
    float3 S_TRT = lightColor * (Mp_TRT * Np_TRT * invCos2ThetaD * cosThetaI);

    // =========================================================
    // 6. 多重散乱ハック (Multiple Scattering / Slide 39)
    // =========================================================
    float3 fakeNormal = normalize(V - T * dot(V, T));
    
    // Wrapped Lambert で影側・裏側にも光を包み込ませる
    float wrap = 0.5f;
    float fakeNdotL = saturate((dot(fakeNormal, L) + wrap) / (1.0f + wrap));
    
    float shadowValue = 1.0f; // シャドウマップ非使用時は 1.0
    float depth = 1.0f - shadowValue;
    float3 scatterAbs = pow(safeColor, float3(1.0f + depth, 1.0f + depth, 1.0f + depth));
    
    // Scatter パラメータでスケール
    float3 S_scatter = scatterAbs * fakeNdotL * lightColor * scatter;

    // =========================================================
    // 7. 全光パスの統合
    // =========================================================
    return (S_R + S_TT + S_TRT + S_scatter) * lightIntensity;
}

// -------------------------------------------------------------
// DXR Ray Tracing Closest Hit Shader
// -------------------------------------------------------------
[shader("closesthit")]
void HairClosestHitShader(inout RayPayload payload, in HairAttribute attribute)
{
    uint32_t aabbIndex = PrimitiveIndex();

    // 頂点データ・BaseColor の取得
    StrandVertex v0 = HairFlatVertices[aabbIndex * 2 + 0];
    StrandVertex v1 = HairFlatVertices[aabbIndex * 2 + 1];
    float3 baseColor = lerp(v0.color, v1.color, attribute.uv.y);
    
    // ワールド空間のベクトルの準備
    float3 localT = normalize(v1.position - v0.position);
    float3 worldT = normalize(mul(ObjectToWorld3x4(), float4(localT, 0.0f)));

    float3 V = normalize(-WorldRayDirection());
    float3 L = normalize(-g_LightingParams.lightDirection);

    // アーティスト用パラメータの設定
    float roughness = 0.35f; // UI Roughness (0.0 ~ 1.0)
    float shift = 0.03f; // UI Shift (約 1.7度)
    float specular = 1.00f; // UI Specular
    float scatter = 1.00f; // UI Scatter

    // 統合シェーダーによる一括ライティング計算
    float3 finalColor = EvaluateHair_Epic_Unified(
        L, V, worldT,
        baseColor,
        roughness,
        shift,
        specular,
        scatter,
        g_LightingParams.lightIntensity,
        g_LightingParams.lightColor
    );
    
    // 環境光 (完全な暗闇を防止)
    float3 ambient = baseColor * 0.05f;
    finalColor += ambient;

    // Reinhard トーンマッピング
    payload.color = finalColor / (finalColor + 1.0f);
}


//[shader("closesthit")]
//void HairClosestHitShader(inout RayPayload payload, in HairAttribute attribute)
//{
//    uint32_t aabbIndex = PrimitiveIndex();
    
//    // 💡 安全対策: バッファの最大要素数を取得して、配列外アクセスによるクラッシュを防ぐ
//    uint32_t numVertices = 0;
//    uint32_t stride = 0;
//    HairFlatVertices.GetDimensions(numVertices, stride);
//    uint32_t maxAABBIndex = (numVertices / 2) - 1;

//    // 現在のセグメントの頂点
//    StrandVertex v0 = HairFlatVertices[aabbIndex * 2 + 0];
//    StrandVertex v1 = HairFlatVertices[aabbIndex * 2 + 1];
    
//    // 現在のセグメントの方向ベクトル（基本軸）
//    float32_t3 W_curr = normalize(v1.position - v0.position);

//    // ------------------------------------------------------------
//    // 💡 魔法の処理1: 前後のセグメントを覗き見て、シームレスな接線 T を作る！
//    // ------------------------------------------------------------
//    float32_t3 T0 = W_curr; // 始点 v0 での接線（初期値）
//    float32_t3 T1 = W_curr; // 終点 v1 での接線（初期値）

//    // ① 【前隣のセグメントとの接続を自動判定】
//    if (aabbIndex > 0)
//    {
//        StrandVertex prev_v1 = HairFlatVertices[(aabbIndex - 1) * 2 + 1];
//        // 頂点座標がほぼ同じ位置なら「同じ髪の毛の続き」とみなす
//        if (distance(prev_v1.position, v0.position) < 0.001f)
//        {
//            StrandVertex prev_v0 = HairFlatVertices[(aabbIndex - 1) * 2 + 0];
//            float32_t3 W_prev = normalize(prev_v1.position - prev_v0.position);
//            T0 = normalize(W_prev + W_curr); // 前後の向きをブレンドして滑らかに繋ぐ
//        }
//    }

//    // ② 【次隣のセグメントとの接続を自動判定】
//    if (aabbIndex < maxAABBIndex)
//    {
//        StrandVertex next_v0 = HairFlatVertices[(aabbIndex + 1) * 2 + 0];
//        // 頂点座標がほぼ同じ位置なら「同じ髪の毛の続き」とみなす
//        if (distance(next_v0.position, v1.position) < 0.001f)
//        {
//            StrandVertex next_v1 = HairFlatVertices[(aabbIndex + 1) * 2 + 1];
//            float32_t3 W_next = normalize(next_v1.position - next_v0.position);
//            T1 = normalize(W_curr + W_next); // 前後の向きをブレンドして滑らかに繋ぐ
//        }
//    }

//    // ③ 【接線の LERP（線形補間）】
//    // 当たった位置 attribute.uv.y (0.0:根元 〜 1.0:毛先) に応じて、接線を完全にブレンド！
//    float32_t3 T = normalize(lerp(T0, T1, attribute.uv.y));

//    // ------------------------------------------------------------
//    // 💡 魔法の処理2: 補間された接線 T に合わせて、法線 N も再直交化する
//    // ------------------------------------------------------------
//    float32_t3 N_raw = attribute.normal;
//    // 新しい滑らかな接線 T に直交する平面へ、法線 N を再投影
//    // これにより、関節部分でも法線がカクッと折れず、完全な曲面が維持されます
//    float32_t3 N = normalize(N_raw - dot(N_raw, T) * T);

//    // ------------------------------------------------------------
//    // 3. ライティング計算 (ここからは前回同様)
//    // ------------------------------------------------------------
//    float32_t3 baseColor = lerp(v0.color, v1.color, attribute.uv.y);
//    float32_t3 L = normalize(g_LightingParams.lightDirection);
    
//    // エッジフェード
//    float32_t distFromCenter = attribute.uv.x;
//    float32_t edgeFade = saturate(1.0f - pow(distFromCenter, 3.0f));

//    float32_t3 V = normalize(-WorldRayDirection());
//    float32_t3 H = normalize(L + V);

//    // Wrap Lambert
//    float32_t wrap = 0.4f;
//    float32_t NdotL = saturate((dot(N, L) + wrap) / (1.0f + wrap));
    
//    float32_t dotTL = dot(T, L);
//    float32_t sinTL = sqrt(max(0.0f, 1.0f - dotTL * dotTL));
//    float32_t3 diffuseColor = baseColor * sinTL * NdotL * g_LightingParams.lightColor * g_LightingParams.lightIntensity;

//    // Specular (天使の輪)
//    float32_t3 shiftedT = normalize(T + N * g_LightingParams.hairShift);
//    float32_t dotTH = dot(shiftedT, H);
//    float32_t sinTH = sqrt(max(0.0f, 1.0f - dotTH * dotTH));
    
//    float32_t specularPower = pow(max(0.0f, sinTH), g_LightingParams.hairExponent);
//    float32_t3 specularColor = g_LightingParams.lightColor * g_LightingParams.lightIntensity * specularPower;

//    // フェード適用
//    diffuseColor *= edgeFade;
//    specularColor *= edgeFade;

//    // 環境光
//    float32_t3 ambient = baseColor * g_LightingParams.ambientColor * edgeFade;
    
//    payload.color = diffuseColor + specularColor + ambient;
//}


//// 物理ベース髪シェーディングのためのヘルパー関数（ガウス分布関数 M）
//// - sinThetaH: dot(T, H) の結果
//// - shift: キューティクルの傾き（ラジアン相当の近似）
//// - roughness: 髪の粗さ
//float32_t Hair_M(float32_t sinThetaH, float32_t shift, float32_t roughness)
//{
//    // ガウス分布の計算（powの代わりになるエネルギー保存型の関数）
//    // 本来は角度(theta)で計算しますが、高速化のため sin(theta) で近似することが多いです
//    float32_t x = sinThetaH - shift;
//    float32_t variance = roughness * roughness;
    
//    // (1.0 / sqrt(2 * PI * variance)) * exp(-(x^2) / (2 * variance))
//    // ※ 0.39894228f は 1.0 / sqrt(2 * PI) の値
//    float32_t amplitude = 0.39894228f / roughness;
//    float32_t exponent = -(x * x) / (2.0f * variance);
    
//    return amplitude * exp(exponent);
//}

//[shader("closesthit")]
//void HairClosestHitShader(inout RayPayload payload, in HairAttribute attribute)
//{
//    uint32_t aabbIndex = PrimitiveIndex();
//    StrandVertex v0 = HairFlatVertices[aabbIndex * 2 + 0];
//    StrandVertex v1 = HairFlatVertices[aabbIndex * 2 + 1];
    
//    float32_t3 baseColor = lerp(v0.color, v1.color, attribute.uv.y);
//    float32_t3 T = normalize(v1.position - v0.position);

//    float32_t3 L = normalize(float32_t3(1.0f, 1.0f, -1.0f));
//    float32_t3 V = normalize(-WorldRayDirection());
//    float32_t3 H = normalize(L + V);
//    float32_t3 N = attribute.normal;
    
//    // ------------------------------------------------------------
//    // PBRパラメータ（これらをマテリアルから渡せるようにするとさらに良いです）
//    // ------------------------------------------------------------
//    float32_t roughness = 0.25f; // 髪の粗さ（β）
//    float32_t shift = 0.05f; // キューティクルの傾き（α）
    
//    // ------------------------------------------------------------
//    // ① Diffuse (TTパス / 多重散乱の近似)
//    // ------------------------------------------------------------
//    // Epic流の「Giant artistic hack」に則り、N（法線）を使ったWrap Lambertを採用
//    // これにより、髪の毛の束全体の柔らかい透過感（ボリューム感）を出します。
//    float32_t wrap = 0.5f;
//    float32_t NdotL = saturate((dot(N, L) + wrap) / ((1.0f + wrap) * (1.0f + wrap)));
//    float32_t3 diffuseColor = baseColor * NdotL;

//    // ------------------------------------------------------------
//    // ② Dual-Specular (Rパス & TRTパス)
//    // ------------------------------------------------------------
//    // 角度のサイン値（LとT、VとT、HとTの内積）
//    float32_t dotTH = dot(T, H);
    
//    // 【第一ハイライト (Rパス)】：キューティクル表面での反射
//    // - shift: 根元側に少しずらす（+shift）
//    // - roughness: 表面の鋭い反射のため狭め（そのまま）
//    float32_t M_R = Hair_M(dotTH, shift, roughness);
    
//    // 簡易的なフレネル（F）と方位角散乱（N_R）の近似
//    // （斜めから見るほど白く強く反射する）
//    float32_t LdotH = saturate(dot(L, H));
//    float32_t fresnel = lerp(0.04f, 1.0f, pow(1.0f - LdotH, 5.0f));
//    float32_t3 specR = float32_t3(1.0f, 1.0f, 1.0f) * M_R * fresnel;

//    // 【第二ハイライト (TRTパス)】：髪の内部透過・反射
//    // - shift: Rとは逆方向（毛先側）に、さらに大きくずらす（-shift * 1.5 等）
//    // - roughness: 内部で散乱するためRより少し広がる（roughness * 2.0 等）
//    float32_t M_TRT = Hair_M(dotTH, -shift * 1.5f, roughness * 2.0f);
    
//    // TRTパスの横方向の散乱（N_TRT）と色の吸収
//    // 内部を通るのでBaseColorが強く乗る（暗い髪ほど吸収されて消える）
//    float32_t3 specTRT = baseColor * M_TRT * 0.8f;

//    // ------------------------------------------------------------
//    // ③ 環境光 ＆ 最終合成
//    // ------------------------------------------------------------
//    float32_t3 ambient = baseColor * 0.05f; // 必要に応じてIBLなどに変更
    
//    payload.color = diffuseColor + specR + specTRT + ambient;
//}

//////////////////////////
////
////  ClosestHit Shader
////
//////////////////////////
//[shader("closesthit")]
//void HairClosestHitShader(inout RayPayload payload, in HairAttribute attribute)
//{
//   // レイトレーシングエンジンが教えてくれる「当たったAABBの通し番号」
//    uint32_t aabbIndex = PrimitiveIndex();
//    StrandVertex v0 = HairFlatVertices[aabbIndex * 2 + 0];
//    StrandVertex v1 = HairFlatVertices[aabbIndex * 2 + 1];
    
//    //// セグメントバッファから、直接このAABBを構成する頂点インデックスを引く
//    //SegmentData seg = HairSegments[aabbIndex];
//    //// 頂点データを直接取得
//    //StrandVertex v0 = HairVertices[seg.v0_Index];
//    //StrandVertex v1 = HairVertices[seg.v1_Index];
    
//    // 当たった場所の比率（attribute.uv.y）を使って、C++からきた頂点カラーを滑らかに補間！
//    float32_t3 baseColor = lerp(v0.color, v1.color, attribute.uv.y);

//    // 固定値 (0,1,0) を廃止し、この節の「本物の傾きベクトル」を計算して接線 T とする！
//    float32_t3 T = normalize(v1.position - v0.position);

//    // 2. 各種ベクトルの準備
//    float32_t3 L = normalize(float32_t3(1.0f, 1.0f, -1.0f)); // ライト方向
//    float32_t3 V = normalize(-WorldRayDirection()); // 視線方向 (レイの逆向き)
//    float32_t3 H = normalize(L + V); // ハーフベクトル（LとVの中間）
//    float32_t3 N = attribute.normal; // Intersectionから渡された法線
    
//    // ------------------------------------------------------------
//    // ① Diffuse (拡散反射) - 髪の毛用のシリンダー（円柱）シェーディング
//    // ------------------------------------------------------------
//    float32_t dotLT = dot(L, T);
//    float32_t diffuse = sqrt(max(0.0f, 1.0f - dotLT * dotLT));
    
//    // 簡易的なライトの回り込み（Wrap-around）を入れて、髪の不透明な影を柔らかくする
//    diffuse = saturate(diffuse * 0.7f + 0.3f);

//    // ------------------------------------------------------------
//    // ② Dual-Specular (天使の輪をリアルにする2重ハイライト)
//    // ------------------------------------------------------------
//    // 髪の表面（キューティクル）の傾きを模倣するため、接線を法線方向に少しシフト（傾斜）させる
//    float32_t3 T1 = normalize(T + N * 0.1f); // 第一ハイライト用（根元側へずれる）
//    float32_t3 T2 = normalize(T - N * 0.05f); // 第二ハイライト用（毛先側へずれる）

//    // 【第一ハイライト (R成分)】: 表面のキューティクルで反射する、シャープで白い光
//    float32_t dotT1H = dot(T1, H);
//    float32_t sinT1H = sqrt(max(0.0f, 1.0f - dotT1H * dotT1H));
//    float32_t spec1 = pow(sinT1H, 80.0f); // 鋭いハイライト（数値大）
//    float32_t3 specularColor1 = float32_t3(1.0f, 1.0f, 1.0f) * spec1 * 0.6f; // 白い光

//    // 【第二ハイライト (TRT成分)】: 髪の内部に入り、後ろに透過して戻ってくる、少し鈍く「髪の色」を帯びた光
//    float32_t dotT2H = dot(T2, H);
//    float32_t sinT2H = sqrt(max(0.0f, 1.0f - dotT2H * dotT2H));
//    float32_t spec2 = pow(sinT2H, 30.0f); // 少し広がるハイライト（数値小）
//    float32_t3 specularColor2 = baseColor * spec2 * 0.4f; // 💡髪の毛の色（baseColor）が乗る！

//    // ------------------------------------------------------------
//    // ③ 環境光 ＆ 最終合成
//    // ------------------------------------------------------------
//    float32_t3 ambient = baseColor * 0.05f;
    
//    // すべてを合算してペイロードに返す
//    payload.color = (baseColor * diffuse) + specularColor1 + specularColor2 + ambient;
//    //payload.color = baseColor;
//}

////////////////////////
//
//     Miss Shader
//
////////////////////////
[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    // 背景はゲームの画像なので特に何もしない
    //payload.color = float32_t3(0.0f, 0.0f, 0.0f);

}
