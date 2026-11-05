#include "Hair.hlsli"

//=================================================================
// 構造体定義
// =================================================================

// 一次レイおよびシャドウレイ共通 Payload
struct RayPayload
{
    float3 color;
    float shadowFactor; // 1.0: 遮蔽なし（日向）, 0.0: 遮蔽あり（影）
};

// Hit Attribute (Intersection -> ClosestHit へ渡す情報)

struct HairAttribute
{
    float2 uv; // x: 中心軸からの距離比率(0~1), y: 節の始点からの位置比率(0~1)
    float3 normal; // ワールド空間における法線
};

// CBV (b0) - カメラ情報
struct RayTracingCamera
{
    float4x4 inverseViewProj;
    float3 position;
    float padding;
};

// SRV (t2) - インスタンス毎のマテリアル＆バッファインデックス
struct RayTracingInstanceMaterial
{
    float4 color;
    uint isHair; // 0: 三角形メッシュ, 1: Hair Procedural
    uint hairBufferIndex; // HairFlatVertices 配列内のインデックス
    float2 padding;
};

struct Ray
{
    float3 Origin;
    float3 Direction;
};

// =================================================================
// リソースバインド (RayTracingManager の Root Signature と一致)
// =================================================================

// b0: カメラ情報
ConstantBuffer<RayTracingCamera> gCamera : register(b0);

// t0: Top-Level Acceleration Structure
RaytracingAccelerationStructure SceneTLAS : register(t0);

// u0: レイトレーシング出力キャンバス
RWTexture2D<float4> gOutput : register(u0);

// t1: シーンの Depth テクスチャ (不透明オブジェクトのクリッピング用)
Texture2D<float> SceneDepth : register(t1);

// t2: インスタンスごとのマテリアル構造体バッファ
StructuredBuffer<RayTracingInstanceMaterial> InstanceMaterials : register(t2);

// t3: 複数髪パーツの頂点バッファ Descriptor Table (Bindless 配列)
StructuredBuffer<StrandVertex> HairFlatVertices[64] : register(t3);

// =================================================================
// シャドウレイ計算用ヘルパー関数
// =================================================================
float CalculateShadow(float3 hitWorldPos, float3 lightDir)
{
    RayPayload shadowPayload;
    shadowPayload.color = float3(0.0f, 0.0f, 0.0f);
    shadowPayload.shadowFactor = 0.0f; // デフォルト：遮蔽されている（影）

    RayDesc shadowRay;
    // 法線方向だけでなく、光の方向にもわずかに押し出す
    float bias = 0.005f;
    shadowRay.Origin = hitWorldPos + lightDir * bias;
    shadowRay.Direction = lightDir;
    shadowRay.TMin = 0.01f; // TMin を少し大きめにして自前交差を回避
    shadowRay.TMax = 10000.0f;

    // シャドウレイの射出:
    // RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH: 最初に何かにヒットした時点で判定終了
    // RAY_FLAG_SKIP_CLOSEST_HIT_SHADER: ClosestHitの実行をスキップして高速化
    // RAY_FLAG_FORCE_OPAQUE: 全てのオブジェクトを不透明扱いとして即時判定
    // 第4引数 (RayContributionToHitGroupIndex) : 0
    // 第5引数 (MultiplierForGeometryContributionToHitGroupIndex) : 1
    // 第6引数 (MissShaderIndex) : 1  <-- ここで Index 1 (ShadowMissShader) を指定！
    TraceRay(
        SceneTLAS,
        RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH |
        RAY_FLAG_SKIP_CLOSEST_HIT_SHADER |
        RAY_FLAG_FORCE_OPAQUE,
        0xFF, 0, 1, 1, shadowRay, shadowPayload
    );

    // 光源までの間に遮蔽物がなく MissShader が呼ばれた場合のみ shadowFactor は 1.0f になる
    return shadowPayload.shadowFactor;
}

// =================================================================
// RayGen Shader
// =================================================================
[shader("raygeneration")]
void MyRayGenShader()
{
    uint2 pixelCoord = DispatchRaysIndex().xy;
    uint2 resolution = DispatchRaysDimensions().xy;
    float4 originalColor = gOutput[pixelCoord];
    
    // UV と NDC の基準を「ピクセル中心 (+0.5f)」に統一
    float2 uv = (float2(pixelCoord) + 0.5f) / float2(resolution);
    float2 ndc = float2(uv.x * 2.0f - 1.0f, (1.0f - uv.y) * 2.0f - 1.0f); // Y軸反転込み

    // レイ方向の算出
    float4 target = mul(float4(ndc, 1.0f, 1.0f), gCamera.inverseViewProj);
    float3 rayDir = normalize(target.xyz / target.w - gCamera.position);

    // デプス値の取得 (0.0: 手前 ~ 1.0: 奥)
    float depth = SceneDepth.Load(int3(pixelCoord, 0));
    
    float maxDistance = 10000.0f;

    // 背景（最遠方 depth >= 0.9999f）以外の場合、奥行き限界距離を計算
    if (depth < 0.9999f && depth > 0.0f)
    {
        float4 clipPos = float4(ndc, depth, 1.0f);
        float4 worldPos = mul(clipPos, gCamera.inverseViewProj);
        worldPos.xyz /= worldPos.w;

        // 浮動小数点誤差で RayTCurrent > TMax になりレイが破棄されるのを防ぐためマージンを付加 (+0.1f)
        maxDistance = length(worldPos.xyz - gCamera.position) * 1.01f + 0.1f;
    }

    RayDesc ray;
    ray.Origin = gCamera.position;
    ray.Direction = rayDir;
    ray.TMin = 0.001f;
    ray.TMax = maxDistance; // マージン付き限界距離

    RayPayload payload;
    payload.color = originalColor.xyz;
    payload.shadowFactor = 1.0f;

    TraceRay(SceneTLAS, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 1, 0, ray, payload);

    gOutput[pixelCoord] = float4(payload.color, 1.0f);
}

// =================================================================
// Triangle Mesh Closest Hit Shader
// =================================================================
[shader("closesthit")]
void MeshClosestHitShader(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    uint instanceID = InstanceID();
    RayTracingInstanceMaterial mat = InstanceMaterials[instanceID];

    float3 hitWorldPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float3 lightDir = normalize(float3(0.5f, 1.0f, -0.5f)); // ハードコードライト方向
    
    // シャドウレイの計算
    float shadow = CalculateShadow(hitWorldPos, lightDir);
    // 影の中でも暗くなりすぎないように環境光マージンを確保 (0.2 ～ 1.0)
    float shadowMask = lerp(0.5f, 1.0f, shadow);

    float3 finalColor = payload.color.rgb * shadowMask;
    payload.color = finalColor;
}

// =================================================================
// Hair Procedural Intersection Shader
// =================================================================
bool RayCapsuleIntersectionTest(Ray ray, float3 p0, float3 p1, float radius, float rayTCurrent, out float thit, out HairAttribute attr)
{
    thit = 0.0f;
    attr = (HairAttribute) 0;

    float3 delta = ray.Origin - p0;
    float3 v = p1 - p0;
    float L = length(v);
    if (L < 0.0001f)
        return false;
    float3 W = v / L;

    float dotDW = dot(ray.Direction, W);
    float denom = 1.0f - dotDW * dotDW;

    float t = 0.0f;
    float u = 0.0f;

    if (denom < 0.0001f)
    {
        t = -dot(delta, ray.Direction);
        u = t * dotDW + dot(delta, W);
    }
    else
    {
        float dotDD = dot(delta, ray.Direction);
        float dotDW_delta = dot(delta, W);
        t = (dotDW_delta * dotDW - dotDD) / denom;
        u = t * dotDW + dotDW_delta;
    }

    if (t > rayTCurrent + radius)
        return false;

    float uClamped = clamp(u, 0.0f, L);
    if (uClamped != u)
    {
        u = uClamped;
        t = -dot(delta - u * W, ray.Direction);
        if (t > rayTCurrent + radius)
            return false;
    }

    float3 hitPointRay = ray.Origin + t * ray.Direction;
    float3 hitPointSegment = p0 + u * W;
    float3 distVec = hitPointRay - hitPointSegment;
    float distSq = dot(distVec, distVec);

    if (distSq <= radius * radius)
    {
        thit = t;
        attr.uv.x = sqrt(distSq) / radius;
        attr.uv.y = u / L;

        float distLengthSq = dot(distVec, distVec);
        if (distLengthSq > 0.000001f)
        {
            attr.normal = distVec / sqrt(distLengthSq);
        }
        else
        {
            attr.normal = -ray.Direction;
        }
        return true;
    }
    return false;
}

[shader("intersection")]
void HairIntersectionShader()
{
    uint instanceID = InstanceID();
    RayTracingInstanceMaterial mat = InstanceMaterials[instanceID];

    // このインスタンスが参照すべき HairFlatVertices の配列インデックス
    uint hairIdx = mat.hairBufferIndex;

    float rayTCurrent = RayTCurrent();
    float rayTMin = RayTMin();

    Ray ray;
    ray.Origin = ObjectRayOrigin();
    ray.Direction = ObjectRayDirection();

    uint aabbIndex = PrimitiveIndex();
    // 該当する髪パーツの頂点バッファ HairFlatVertices[hairIdx] から頂点を取得
    StrandVertex v0 = HairFlatVertices[hairIdx][aabbIndex * 2 + 0];
    StrandVertex v1 = HairFlatVertices[hairIdx][aabbIndex * 2 + 1];

    float radius = v0.radius;

    // 前処理：完全に rayTCurrent より奥にある AABB の早期カリング
    float3 toV0 = v0.position - ray.Origin;
    float3 toV1 = v1.position - ray.Origin;
    float minDistEstimate = min(dot(toV0, ray.Direction), dot(toV1, ray.Direction)) - radius;

    if (minDistEstimate > rayTCurrent)
    {
        return;
    }

    float thit;
    HairAttribute attr;

    if (RayCapsuleIntersectionTest(ray, v0.position, v1.position, radius, rayTCurrent, thit, attr))
    {
        if (thit > rayTMin && thit < rayTCurrent)
        {
            attr.normal = normalize(mul((float3x3) ObjectToWorld3x4(), attr.normal));
            ReportHit(thit, 0, attr);
        }
    }
}

// =================================================================
// 髪の毛 Marschner / Epic Games Unified ライティング関数群
// =================================================================
float3 EvaluateHair_Epic_Unified(
    float3 L,
    float3 V,
    float3 T,
    float3 baseColor,
    float roughness,
    float shift,
    float specular,
    float scatter,
    float lightIntensity,
    float3 lightColor
)
{

    float betaR = max(0.001f, roughness * roughness);

    float betaTT = 0.5f * betaR;

    float betaTRT = 2.0f * betaR;



    float alphaR = -2.0f * shift;

    float alphaTT = 1.0f * shift;

    float alphaTRT = 4.0f * shift;



    float sinThetaI = clamp(dot(L, T), -0.999f, 0.999f);

    float sinThetaR = clamp(dot(V, T), -0.999f, 0.999f);

    float cosThetaI = sqrt(max(0.0f, 1.0f - sinThetaI * sinThetaI));

    float cosThetaR = sqrt(max(0.0f, 1.0f - sinThetaR * sinThetaR));



    float cosThetaD = sqrt(max(0.001f, 0.5f * (1.0f + cosThetaI * cosThetaR + sinThetaI * sinThetaR)));

    float invCos2ThetaD = 1.0f / (cosThetaD * cosThetaD);



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



    float F0 = 0.046f;

    float Fresnel = F0 + (1.0f - F0) * pow(saturate(1.0f - cosThetaD), 5.0f);

    float3 safeColor = max(float3(0.001f, 0.001f, 0.001f), baseColor);



    // R パス (一次反射)

    float shiftR = sinThetaI + sinThetaR - alphaR;

    float Mp_R = (1.0f / (betaR * 2.50662827f)) * exp(-(shiftR * shiftR) / (2.0f * betaR * betaR));

    float Np_R = cosHalfPhi;

    float3 S_R = lightColor * (Mp_R * Np_R * Fresnel * invCos2ThetaD * cosThetaI) * specular;



    // TT パス (透過光)

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



    // TRT パス (二次内部反射)

    float shiftTRT = sinThetaI + sinThetaR - alphaTRT;

    float Mp_TRT = (1.0f / (betaTRT * 2.50662827f)) * exp(-(shiftTRT * shiftTRT) / (2.0f * betaTRT * betaTRT));

    float D_TRT = exp(17.0f * cosPhi - 16.78f);

    float3 Abs_TRT = pow(safeColor, float3(0.8f / cosThetaD, 0.8f / cosThetaD, 0.8f / cosThetaD));

    float F_TRT = Fresnel * (1.0f - Fresnel) * (1.0f - Fresnel);

    float3 Np_TRT = D_TRT * F_TRT * Abs_TRT;

    float3 S_TRT = lightColor * (Mp_TRT * Np_TRT * invCos2ThetaD * cosThetaI);



    // 多重散乱

    float3 fakeNormal = normalize(V - T * dot(V, T));

    float wrap = 0.5f;

    float fakeNdotL = saturate((dot(fakeNormal, L) + wrap) / (1.0f + wrap));

    float3 S_scatter = safeColor * fakeNdotL * lightColor * scatter;



    return (S_R + S_TT + S_TRT + S_scatter) * lightIntensity;

}

// =================================================================
// Hair Closest Hit Shader
// =================================================================

[shader("closesthit")]
void HairClosestHitShader(inout RayPayload payload, in HairAttribute attribute)
{
    uint instanceID = InstanceID();
    RayTracingInstanceMaterial mat = InstanceMaterials[instanceID];
    uint hairIdx = mat.hairBufferIndex;
    uint aabbIndex = PrimitiveIndex();

    // 衝突ワールド位置の取得
    float3 hitWorldPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    // 該当する髪パーツの頂点バッファから頂点色を取得
    StrandVertex v0 = HairFlatVertices[hairIdx][aabbIndex * 2 + 0];
    StrandVertex v1 = HairFlatVertices[hairIdx][aabbIndex * 2 + 1];
    float3 baseColor = lerp(v0.color, v1.color, attribute.uv.y);

    if (length(mat.color.rgb) > 0.001f)
    {
        baseColor *= mat.color.rgb;
    }

    float3 localT = normalize(v1.position - v0.position);
    float3 worldT = normalize(mul((float3x3) ObjectToWorld3x4(), localT));

    float3 V = normalize(-WorldRayDirection());

    // ハードコードされたライトパラメータ
    float3 L = normalize(float3(0.0f, 0.0f, -0.1f));
    float3 lightColor = float3(1.0f, 1.0f, 1.0f);
    float lightIntensity = 2.0f;

    // シャドウレイの判定
    float shadow = CalculateShadow(hitWorldPos, L);

    // 髪の毛の影の落ち具合（影の中でも暗くなりすぎないよう調整: 0.25 ～ 1.0）
    float shadowMask = lerp(0.25f, 1.0f, shadow);
    float roughness = 0.35f;
    float shift = 0.03f;
    float specular = 1.00f;
    float scatter = 1.00f;

    // 直接光の計算
    float3 directColor = EvaluateHair_Epic_Unified(
        L, V, worldT,
        baseColor,
        roughness,
        shift,
        specular,
        scatter,
        lightIntensity * shadowMask, // 影のマスクを乗算
        lightColor
    );

    // 散乱環境光
    float3 ambientLight = float3(0.1f, 0.1f, 0.12f);
    float3 upDir = float3(0.0f, 1.0f, 0.0f);
    float hemi = saturate(dot(worldT, upDir) * 0.5f + 0.5f);
    float3 ambientScattering = baseColor * ambientLight * lerp(0.8f, 1.2f, hemi);

    float3 finalColor = directColor + ambientScattering;
    // トーンマッピング
    payload.color = finalColor / (finalColor + 1.0f);
}

// =================================================================
// Miss Shader
// =================================================================

// SBT Miss Index 0: カメラレイ（一次レイ）用 Miss Shader
[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    // 背景は透過（ラスタライズ結果を保持）
}

// SBT Miss Index 1: シャドウレイ用 Miss Shader
[shader("miss")]
void ShadowMissShader(inout RayPayload payload)
{
    // 光源まで遮蔽物なく到達できた（何にも当たらなかった）場合のみ 1.0f (日向) に更新
    payload.shadowFactor = 1.0f;
}