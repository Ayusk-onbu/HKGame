#include "Hair.hlsli"
#include "../SDFFunction.hlsli"

//struct SDFInfo
//{
//    float3 gridMin; // SDFバウンディングボックスの最小点
//    float pad0;
//    float3 gridMax; // SDFバウンディングボックスの最大点
//    float pad1;
//    uint3 resolution; // テクスチャ解像度 (例: 64, 64, 64)
//    uint totalNodes;
//};

//ConstantBuffer<HairPhysicsConfig> gHairPhysicsConfig : register(b0);
//ConstantBuffer<FrameConfig> gFrameConfig : register(b1);
ConstantBuffer<HairMaterialParameter> gHairMaterialParm : register(b0);
ConstantBuffer<HairSceneParameter> gHairSceneParm : register(b1);
ConstantBuffer<HairConfig> gHairConfig : register(b2);

cbuffer SDFFunctionInfo : register(b3)
{
    uint32_t activeInstanceCount;
}

//ConstantBuffer<SDFInfo> g_SDFConfig : register(b3);
StructuredBuffer<GuideInfo> g_GuideInfos : register(t0);
//Texture3D<float> g_SDFTexture : register(t1); // SDFManager から渡す SRV
StructuredBuffer<SDFFunctionInstanceGPU> g_SDFFunctionInstances : register(t1);
StructuredBuffer<SDFFunctionStackNodeGPU> g_SDFFunctionNodes : register(t2);
RWStructuredBuffer<ControllerPoint> g_GuideBuffer : register(u0);
SamplerState g_LinearSampler : register(s0);

// -------------------------------------------------------------
// 中心差分による SDF 法線（押し出し方向）の計算
// -------------------------------------------------------------
float3 CalcSDFNormal(SDFEvaluator sdf, float3 p)
{
    const float eps = 0.002f;
    const float2 k = float2(1.0f, -1.0f);
    int32_t dummy;

    float3 grad = k.xyy * sdf.EvaluateScene(p + k.xyy * eps, activeInstanceCount, dummy)
                + k.yyx * sdf.EvaluateScene(p + k.yyx * eps, activeInstanceCount, dummy)
                + k.yxy * sdf.EvaluateScene(p + k.yxy * eps, activeInstanceCount, dummy)
                + k.xxx * sdf.EvaluateScene(p + k.xxx * eps, activeInstanceCount, dummy);

    float lenSq = dot(grad, grad);
    return (lenSq > 1e-10f) ? (grad * rsqrt(lenSq)) : float3(0.0f, 1.0f, 0.0f);
}

// -------------------------------------------------------------
// 解析的 SDF 衝突解決
// -------------------------------------------------------------
bool ResolveSDFCollision(SDFEvaluator sdf, inout float3 position, float hairRadius)
{
    int32_t hitIdx = -1;
    float dist = sdf.EvaluateScene(position, activeInstanceCount, hitIdx);

    // 表面より内側、または髪の太さ分めり込んでいたら押し出す
    if (dist < hairRadius)
    {
        float3 normal = CalcSDFNormal(sdf, position);
        float pushAmount = hairRadius - dist;
        position += normal * pushAmount;
        return true;
    }
    return false;
}

#define MAX_GUIDE_POINTS 64

// -------------------------------------------------------------
// メインコンピュートシェーダー
// -------------------------------------------------------------
[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint guideIdx = dispatchThreadID.x;
    if (guideIdx >= gHairConfig.numGuides)
        return;

    GuideInfo info = g_GuideInfos[guideIdx];
    uint count = min(info.vertexCount, MAX_GUIDE_POINTS);
    if (count < 2)
        return;

    float dt = max(gHairSceneParm.gDeltaTime, 0.0001f);
    float invDt = 1.0f / dt;
    float dtSq = dt * dt;

    // VRAMアクセスを最小限にするためのレジスタ配列
    float3 pos[MAX_GUIDE_POINTS];
    //float3 oldPos[MAX_GUIDE_POINTS];
    float3 basePos[MAX_GUIDE_POINTS];
    float3 vel[MAX_GUIDE_POINTS];
    float invMass[MAX_GUIDE_POINTS];
    float nextLen[MAX_GUIDE_POINTS];
    float radius[MAX_GUIDE_POINTS];
    float bendFactor[MAX_GUIDE_POINTS];

    // ループ不変定数の事前計算
    float stiffness = max(gHairMaterialParm.gStiffness * 2000.0f, 1000.0f);
    float distAlpha = 1.0f / (stiffness * dtSq);
    float invVertexCountMinus1 = 1.0f / (float) (count - 1);

    float currentAccumulatedLength = 0.0f;
    float3 headCenter = mul(gHairMaterialParm.gCharacterMatrix, float4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;

    // キャラの移動方向と慣性力
    float moveLenSq = dot(gHairMaterialParm.gMoveDirection, gHairMaterialParm.gMoveDirection);
    float moveLen = (moveLenSq > 1e-8f) ? sqrt(moveLenSq) : 0.0f;
    float3 moveDir = (moveLen > 0.0001f) ? (gHairMaterialParm.gMoveDirection / moveLen) : float3(0.0f, 0.0f, 0.0f);
    float3 inertia = -moveDir * min(moveLen, 0.05f) * 0.05f;
    float3 charVel = (gHairMaterialParm.gMoveDirection * invDt);
    float3 charMove = gHairMaterialParm.gMoveDirection;
    
    const float followRatio = 0.98f;

    SDFEvaluator sdf;
    sdf.instances = g_SDFFunctionInstances;
    sdf.nodes = g_SDFFunctionNodes;

    // =========================================================
    // ステップ1: 一括ロード ＆ 位置予測 ＆ 根元の追従
    // =========================================================
    for (uint i = 0; i < count; ++i)
    {
        uint currentIdx = info.vertexStartIndex + i;
        ControllerPoint p = g_GuideBuffer[currentIdx];

        //oldPos[i] = p.position;
        nextLen[i] = p.nextToLength;
        radius[i] = p.radius;

        float strandRatio = (float) i * invVertexCountMinus1;
        float oneMinusRatio = 1.0f - strandRatio;

        // 曲げ剛性（restoringForce = 0.5 前後でしなやかに曲がる）
        float softnessFactor = pow(max(oneMinusRatio, 0.0f), 1.5f);
        float bendStiffness = max(gHairMaterialParm.gRestoringForce * 5.0f * softnessFactor, 0.001f);
        float bendAlpha = 1.0f / (bendStiffness * dtSq);
        bendFactor[i] = 1.0f / (1.0f + bendAlpha);

        if (p.physicsWeight <= 0.0001f)
        {
            invMass[i] = 0.0f;
            float3 rootPos = mul(gHairMaterialParm.gCharacterMatrix, float4(p.homePosition, 1.0f)).xyz;
            basePos[i] = rootPos;
            pos[i] = rootPos;
            vel[i] = float3(0.0f, 0.0f, 0.0f);
        }
        else
        {
            invMass[i] = 1.0f;
            float3 targetHomeWorld = mul(gHairMaterialParm.gCharacterMatrix, float4(p.homePosition, 1.0f)).xyz;

            if (i > 0)
            {
                currentAccumulatedLength += nextLen[i - 1];
            }

            float wavePhase = gHairSceneParm.gTime * 6.0f - currentAccumulatedLength * 15.0f;

            // 1. 重力（-0.98 * 10.0 = -9.8）
            float3 gravity = gHairSceneParm.gGravity * p.physicsWeight * 10.0f;

            // 2. 風
            float3 wind = gHairSceneParm.gWindDirection * (sin(wavePhase) * 0.5f + 0.5f) * p.physicsWeight;

            // 3. 復元力（根元付近だけ効かせ、毛先は自由に動かす）
            float restoringScale = gHairMaterialParm.gRestoringForce * (oneMinusRatio * oneMinusRatio);
            float3 toHome = (targetHomeWorld - p.position) * restoringScale;

            // 4. ★ キャラの移動による空気抵抗（向かい風・上昇気流）
            // キャラが下へ落ちる(charVel.y < 0)と、上向き(+Y)の強い風を受けて重力を打ち消す
            float3 relativeAirVel = -(p.velocity + charVel);
            float relSpeedSq = dot(relativeAirVel, relativeAirVel);
            float3 airDrag = (relSpeedSq > 1e-8f)
                ? (relativeAirVel * sqrt(relSpeedSq) * 0.4f * p.physicsWeight)
                : float3(0.0f, 0.0f, 0.0f);

            // 落下時はさらに上向きの浮き上がり補正を強める（重力 -9.8 をしっかり上回る値にする）
            float fallSpeed = max(-charVel.y, 0.0f);
            float3 fallLift = float3(0.0f, 1.0f, 0.0f) * min(fallSpeed * 6.0f, 25.0f) * strandRatio;

            // 5. 頭部中心からの拡散力（落下中は空気をはらんで外側にブワッと広がる）
            float3 outwardDir = p.position - headCenter;
            float outwardLenSq = dot(outwardDir, outwardDir);
            float3 spreadDir = (outwardLenSq > 1e-8f) ? (outwardDir * rsqrt(outwardLenSq)) : float3(0.0f, 0.0f, 0.0f);
            float fallSpreadBoost = 1.0f + min(fallSpeed * 0.5f, 3.0f);
            float3 spreadForce = spreadDir * (gHairMaterialParm.gGlobalSpreadRadius * 12.0f * fallSpreadBoost) * strandRatio;

            float3 totalForce = gravity + wind + toHome + airDrag + fallLift + spreadForce;

            // 全軸同じ追従率でベース位置を更新
            float3 followedPos = p.position + charMove * followRatio;
            basePos[i] = followedPos;

            // 速度更新と予測位置の算出
            float3 v = (p.velocity + totalForce * dt) * gHairMaterialParm.gDamping;
            float vLenSq = dot(v, v);
            if (vLenSq > 64.0f) // 上限 8.0f
            {
                v *= 8.0f * rsqrt(vLenSq);
            }

            vel[i] = v;
            pos[i] = followedPos + v * dt;
        }
    }

    // =========================================================
    // ステップ2: XPBDソルバーループ（レジスタ上で完結）
    // =========================================================
    for (uint iter = 0; iter < 20; ++iter)
    {
        // XPBD 距離制約
        for (uint j = 1; j < count; ++j)
        {
            float w1 = invMass[j - 1];
            float w2 = invMass[j];
            float wSum = w1 + w2;
            if (wSum <= 0.0f)
                continue;

            float3 delta = pos[j] - pos[j - 1];
            float lenSq = dot(delta, delta);
            if (lenSq < 1e-8f)
                continue;

            float currentLength = sqrt(lenSq);
            float C = currentLength - nextLen[j - 1];
            float deltaLambda = -C / (wSum + distAlpha);
            float3 corr = delta * (deltaLambda / currentLength);

            pos[j - 1] -= corr * w1;
            pos[j] += corr * w2;
        }

        // XPBD 曲げ制約
        for (uint b = 1; b < count - 1; ++b)
        {
            if (invMass[b] > 0.0f)
            {
                float3 center = (pos[b - 1] + pos[b + 1]) * 0.5f;
                pos[b] += (center - pos[b]) * bendFactor[b];
            }
        }

        // SDF 衝突解決（4回に1回＋最終反復で実行し、負荷を1/4に削減しつつ収束を維持）
        if (activeInstanceCount > 0 && ((iter & 3) == 3))
        {
            for (uint c = 1; c < count; ++c)
            {
                if (invMass[c] > 0.0f)
                {
                    ResolveSDFCollision(sdf, pos[c], radius[c]);
                }
            }
        }
    }

    // =========================================================
    // ステップ3: 補正実績から「速度」を逆算して一括書き戻し
    // =========================================================
    for (uint k = 0; k < count; ++k)
    {
        uint idx = info.vertexStartIndex + k;

        if (invMass[k] > 0.0f)
        {
            //float3 worldVel = (pos[k] - oldPos[k]) * invDt;
            float3 shakeVel = (pos[k] - basePos[k]) * invDt;

            float velLenSq = dot(shakeVel, shakeVel);
            if (velLenSq > 25.0f)
            {
                shakeVel *= 5.0f * rsqrt(velLenSq);
            }

            g_GuideBuffer[idx].position = pos[k];
            g_GuideBuffer[idx].velocity = shakeVel;
        }
        else
        {
            g_GuideBuffer[idx].position = pos[k];
            g_GuideBuffer[idx].velocity = float3(0.0f, 0.0f, 0.0f);
        }
    }
}