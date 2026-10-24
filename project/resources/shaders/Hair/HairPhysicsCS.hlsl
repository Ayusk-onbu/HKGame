#include "Hair.hlsli"

struct SDFInfo
{
    float3 gridMin; // SDFバウンディングボックスの最小点
    float pad0;
    float3 gridMax; // SDFバウンディングボックスの最大点
    float pad1;
    uint3 resolution; // テクスチャ解像度 (例: 64, 64, 64)
    uint totalNodes;
};

//ConstantBuffer<HairPhysicsConfig> gHairPhysicsConfig : register(b0);
//ConstantBuffer<FrameConfig> gFrameConfig : register(b1);
ConstantBuffer<HairMaterialParameter> gHairMaterialParm : register(b0);
ConstantBuffer<HairSceneParameter> gHairSceneParm : register(b1);
ConstantBuffer<HairConfig> gHairConfig : register(b2);
ConstantBuffer<SDFInfo> g_SDFConfig : register(b3);
StructuredBuffer<GuideInfo> g_GuideInfos : register(t0);
Texture3D<float> g_SDFTexture : register(t1); // SDFManager から渡す SRV
RWStructuredBuffer<ControllerPoint> g_GuideBuffer : register(u0);
SamplerState g_LinearSampler : register(s0);

// -------------------------------------------------------------
// SDF 衝突処理
// -------------------------------------------------------------
bool ResolveSDFCollision(inout float3 position, float hairRadius)
{
    float3 gridExtent = g_SDFConfig.gridMax - g_SDFConfig.gridMin;
    float3 uvw = (position - g_SDFConfig.gridMin) / gridExtent;

    if (any(uvw < 0.0f) || any(uvw > 1.0f))
        return false;

    float dist = g_SDFTexture.SampleLevel(g_LinearSampler, uvw, 0);

    if (dist < hairRadius)
    {
        float3 delta = 1.5f / (float3) g_SDFConfig.resolution;
        float dX1 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw + float3(delta.x, 0, 0), 0);
        float dX2 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw - float3(delta.x, 0, 0), 0);
        float dY1 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw + float3(0, delta.y, 0), 0);
        float dY2 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw - float3(0, delta.y, 0), 0);
        float dZ1 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw + float3(0, 0, delta.z), 0);
        float dZ2 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw - float3(0, 0, delta.z), 0);

        float3 gradient = float3(
            (dX1 - dX2) / gridExtent.x,
            (dY1 - dY2) / gridExtent.y,
            (dZ1 - dZ2) / gridExtent.z
        );

        float gradLen = length(gradient);
        if (gradLen < 0.001f)
            return false;

        float3 normal = gradient / gradLen;
        float pushAmount = hairRadius - dist;
        position += normal * pushAmount * 1.0f;

        return true;
    }
    return false;
}

// -------------------------------------------------------------
// XPBD 距離制約 (Distance Constraint)
// -------------------------------------------------------------
void ApplyXPBDDistanceConstraint(uint parentIdx, uint childIdx, float dt)
{
    ControllerPoint parent = g_GuideBuffer[parentIdx];
    ControllerPoint child = g_GuideBuffer[childIdx];

    float3 delta = child.position - parent.position;
    float currentLength = length(delta);
    if (currentLength < 0.0001f)
        return;

    float targetLength = parent.nextToLength;
    float C = currentLength - targetLength; // ズレ量

    // 逆質量
    float w1 = (parent.physicsWeight <= 0.0001f) ? 0.0f : 1.0f;
    float w2 = (child.physicsWeight <= 0.0001f) ? 0.0f : 1.0f;
    float wSum = w1 + w2;
    if (wSum <= 0.0001f)
        return;

    // XPBD コンプライアンス (1 / (k * dt^2))
    float stiffness = max(gHairMaterialParm.gStiffness * 2000.0f, 1000.0f);
    float alphaTilde = 1.0f / (stiffness * dt * dt);

    float deltaLambda = -C / (wSum + alphaTilde);
    float3 dir = delta / currentLength;

    if (w1 > 0.0f)
        g_GuideBuffer[parentIdx].position -= dir * (w1 * deltaLambda);
    if (w2 > 0.0f)
        g_GuideBuffer[childIdx].position += dir * (w2 * deltaLambda);
}

// -------------------------------------------------------------
// XPBD 曲げ制約 (Bending Constraint)
// -------------------------------------------------------------
void ApplyXPBDBendingConstraint(uint prevIdx, uint currIdx, uint nextIdx, float dt)
{
    ControllerPoint curr = g_GuideBuffer[currIdx];
    if (curr.physicsWeight <= 0.0001f)
        return;

    float3 p0 = g_GuideBuffer[prevIdx].position;
    float3 p1 = curr.position;
    float3 p2 = g_GuideBuffer[nextIdx].position;

    float3 center = (p0 + p2) * 0.5f;
    float3 diff = center - p1;

    float bendStiffness = max(gHairMaterialParm.gRestoringForce * 100.0f, 0.01f);
    float alphaTilde = 1.0f / (bendStiffness * dt * dt);

    float deltaLambda = 1.0f / (1.0f + alphaTilde);
    g_GuideBuffer[currIdx].position += diff * deltaLambda;
}

// -------------------------------------------------------------
// メインコンピュートシェーダー
// -------------------------------------------------------------
[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    // 計算する対象のガイドのIndexを取得
    uint guideIdx = dispatchThreadID.x;
    // 対象外の場合は終了
    if (guideIdx >= gHairConfig.numGuides)
        return;
    // 対象の情報を取得
    GuideInfo info = g_GuideInfos[guideIdx];
    // 構成する頂点数が一つの場合は終了
    if (info.vertexCount < 2)
        return;

    float dt = max(gHairSceneParm.gDeltaTime, 0.0001f);
    // 現在の髪の全長の計算用
    float currentAccumulatedLength = 0.0f;

    // ==============================
    // 【 キャラの移動方向と慣性力 】
    // ==============================
    // 移動量の大きさを取得
    float moveLen = length(gHairMaterialParm.gMoveDirection);
    // 移動の向きを取得
    float3 moveDir = (moveLen > 0.0001f) ? (gHairMaterialParm.gMoveDirection / moveLen) : float3(0, 0, 0);
    // 慣性力の計算
    float3 inertia = -moveDir * min(moveLen, 0.5f) * 0.125f;

    // 速度逆算用の旧位置保持配列
    float3 oldPositions[512];

    // =========================================================
    // ステップ1: 位置予測 ＆ 根元の追従
    // =========================================================
    for (uint i = 0; i < info.vertexCount; ++i)
    {
        // 計算する対象の頂点のIndex取得
        uint currentIdx = info.vertexStartIndex + i;
        // 対象の頂点の情報
        ControllerPoint p = g_GuideBuffer[currentIdx];
        // 速度計算用に現在のワールド位置を保存
        oldPositions[i] = p.position;

        if (p.physicsWeight <= 0.0001f)
        {
            // 【根元】物理演算をスキップし、キャラの最新ワールド位置へ強制ワープさせる
            p.position = mul(gHairMaterialParm.gCharacterMatrix, float4(p.homePosition, 1.0f)).xyz;
            p.velocity = float3(0, 0, 0);
            g_GuideBuffer[currentIdx] = p;
        }
        else
        {
            // 【根本以外】現在のキャラ姿勢におけるワールド目標復元位置を計算
            float3 targetHomeWorld = mul(gHairMaterialParm.gCharacterMatrix, float4(p.homePosition, 1.0f)).xyz;
            // 現在の頂点は髪の相対位置がどこら辺に存在するのかの計算
            currentAccumulatedLength += g_GuideBuffer[currentIdx - 1].nextToLength;
            // Wave処理
            float wavePhase = gHairSceneParm.gTime * 6.0f - currentAccumulatedLength * 15.0f;

            // ==============
            // 【 各種外力 】
            // ==============
            // 重力
            float3 gravity = gHairSceneParm.gGravity * p.physicsWeight;
            // 風
            float3 wind = gHairSceneParm.gWindDirection * sin(wavePhase) * p.physicsWeight;
            // 復元力
            float3 toHome = (targetHomeWorld - p.position) * gHairMaterialParm.gRestoringForce;

            float3 totalForce = gravity + wind + toHome;

            // 仮位置（Predict Position）の計算と更新
            p.velocity = (p.velocity + (inertia + totalForce) * dt) * gHairMaterialParm.gDamping;
            p.position += p.velocity * dt;

            g_GuideBuffer[currentIdx] = p;
        }
    }

    // =========================================================
    // ステップ2: XPBDソルバーループ (ワールド空間)
    // =========================================================
    for (uint iter = 0; iter < 100; ++iter)
    {
        // XPBD 距離制約
        for (uint j = 1; j < info.vertexCount; ++j)
        {
            uint pIdx = info.vertexStartIndex + j - 1;
            uint cIdx = info.vertexStartIndex + j;
            ApplyXPBDDistanceConstraint(pIdx, cIdx, dt);
        }

        // XPBD 曲げ制約
        for (uint b = 1; b < info.vertexCount - 1; ++b)
        {
            uint prev = info.vertexStartIndex + b - 1;
            uint curr = info.vertexStartIndex + b;
            uint next = info.vertexStartIndex + b + 1;
            ApplyXPBDBendingConstraint(prev, curr, next, dt);
        }

        // SDF 衝突解決
        for (uint c = 1; c < info.vertexCount; ++c)
        {
            uint idx = info.vertexStartIndex + c;
            float3 pos = g_GuideBuffer[idx].position;
            float hairRadius = g_GuideBuffer[idx].radius;

            if (ResolveSDFCollision(pos, hairRadius))
            {
                g_GuideBuffer[idx].position = pos;
            }
        }
    }

    // =========================================================
    // ステップ3: 補正実績から「速度」を逆算して保存
    // =========================================================
    for (uint k = 0; k < info.vertexCount; ++k)
    {
        uint idx = info.vertexStartIndex + k;
        ControllerPoint p = g_GuideBuffer[idx];

        if (p.physicsWeight > 0.0001f)
        {
            float3 calculatedVel = (p.position - oldPositions[k]) / dt;

            // 超高速移動時の速度爆発ガード（上限を設ける）
            float velLen = length(calculatedVel);
            if (velLen > 10.0f)
            {
                calculatedVel = (calculatedVel / velLen) * 10.0f;
            }

            p.velocity = calculatedVel;
            g_GuideBuffer[idx] = p;
        }
    }
}

//// -------------------------------------------------------------
//// SDF から距離と押し出し法線（勾配）を計算する関数
//// -------------------------------------------------------------
//bool ResolveSDFCollision(inout float3 position, float hairRadius)
//{
//    float3 gridExtent = g_SDFConfig.gridMax - g_SDFConfig.gridMin;
//    float3 uvw = (position - g_SDFConfig.gridMin) / gridExtent;

//    if (any(uvw < 0.0f) || any(uvw > 1.0f))
//        return false;

//    float dist = g_SDFTexture.SampleLevel(g_LinearSampler, uvw, 0);

//    if (dist < hairRadius)
//    {
//        float3 delta = 1.5f / (float3) g_SDFConfig.resolution;
        
//        float dX1 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw + float3(delta.x, 0, 0), 0);
//        float dX2 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw - float3(delta.x, 0, 0), 0);
//        float dY1 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw + float3(0, delta.y, 0), 0);
//        float dY2 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw - float3(0, delta.y, 0), 0);
//        float dZ1 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw + float3(0, 0, delta.z), 0);
//        float dZ2 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw - float3(0, 0, delta.z), 0);

//        float3 gradient = float3(
//            (dX1 - dX2) / gridExtent.x,
//            (dY1 - dY2) / gridExtent.y,
//            (dZ1 - dZ2) / gridExtent.z
//        );

//        float gradLen = length(gradient);
        
//        // 勾配が小さすぎる（真中心や鞍点など）場合は無理に弾かずスキップ
//        if (gradLen < 0.001f)
//            return false;

//        float3 normal = gradient / gradLen;

//        // 本来押し出すべき量
//        float pushAmount = hairRadius - dist;

//        // 押し出し適用
//        position += normal * pushAmount * 1.00f;

//        return true;
//    }

//    return false;
//}

//// SDF距離と法線の取得
//float SampleSDF(float3 pos, out float3 normal)
//{
//    float3 gridExtent = g_SDFConfig.gridMax - g_SDFConfig.gridMin;
//    float3 uvw = (pos - g_SDFConfig.gridMin) / gridExtent;
//    normal = float3(0, 1, 0);

//    if (any(uvw < 0.0f) || any(uvw > 1.0f))
//        return 999.0f;

//    float dist = g_SDFTexture.SampleLevel(g_LinearSampler, uvw, 0);

//    // 1.5ボクセル幅の差分法線で急激なフリップを抑える
//    float3 delta = 1.0f / (float3) g_SDFConfig.resolution;
//    float dX1 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw + float3(delta.x, 0, 0), 0);
//    float dX2 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw - float3(delta.x, 0, 0), 0);
//    float dY1 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw + float3(0, delta.y, 0), 0);
//    float dY2 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw - float3(0, delta.y, 0), 0);
//    float dZ1 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw + float3(0, 0, delta.z), 0);
//    float dZ2 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw - float3(0, 0, delta.z), 0);

//    float3 grad = float3((dX1 - dX2) / gridExtent.x, (dY1 - dY2) / gridExtent.y, (dZ1 - dZ2) / gridExtent.z);
//    float gradLen = length(grad);
//    if (gradLen > 0.0001f)
//        normal = grad / gradLen;

//    return dist;
//}

//// -------------------------------------------------------------
//// PBD 曲げ拘束（SDFによる急激な屈曲や反転を抑え込む）
//// -------------------------------------------------------------
//void ApplyBendingConstraint(uint prevIdx, uint currIdx, uint nextIdx, float stiffness)
//{
//    float3 p0 = g_GuideBuffer[prevIdx].position;
//    float3 p1 = g_GuideBuffer[currIdx].position;
//    float3 p2 = g_GuideBuffer[nextIdx].position;

//    // 理想的な中点（p0 と p2 の真ん中）
//    float3 center = (p0 + p2) * 0.5f;

//    // p1 が中点からどれだけズレているか（曲がり具合）
//    float3 diff = center - p1;

//    // SDFがp1だけを変な方向に突発的に弾いても、剛性（stiffness: 0.1~0.3程度）で引き戻して滑らかにする
//    g_GuideBuffer[currIdx].position += diff * stiffness;
//}

//// -------------------------------------------------------------
//// PBD 距離拘束（長さの補正）
//// -------------------------------------------------------------
//void ApplyConstraintSpring(uint parentIdx, uint childIdx)
//{
//    float3 posParent = g_GuideBuffer[parentIdx].position;
//    float3 posChild = g_GuideBuffer[childIdx].position;
//    float targetLength = g_GuideBuffer[parentIdx].nextToLength;

//    float3 delta = posChild - posParent;
//    float currentLength = length(delta);

//    if (currentLength > 0.0001f)
//    {
//        float invLength = 1.0f / max(currentLength, 0.0001f);
//        float diff = (currentLength - targetLength) * invLength;
//        float3 correction = delta * diff * 0.5f;

//        // 根元側が固定されている場合
//        if (g_GuideBuffer[parentIdx].physicsWeight <= 0.0001f)
//        {
//            g_GuideBuffer[childIdx].position -= correction * 2.0f;
//        }
//        else
//        {
//            g_GuideBuffer[parentIdx].position += correction;
//            g_GuideBuffer[childIdx].position -= correction;
//        }
//    }
//}

//// -------------------------------------------------------------
//// PBD 距離拘束（根元固定・毛先押し出し）
//// -------------------------------------------------------------
//void ApplyConstraint(uint parentIdx, uint childIdx)
//{
//    float3 posParent = g_GuideBuffer[parentIdx].position;
//    float3 posChild = g_GuideBuffer[childIdx].position;
//    float targetLength = g_GuideBuffer[parentIdx].nextToLength;

//    float3 delta = posChild - posParent;
//    float currentLength = length(delta);

//    if (currentLength > 0.0001f)
//    {
//        // 方向ベクトルを求めて、親から正確な targetLength の位置に子を配置する
//        float3 dir = delta / currentLength;
//        g_GuideBuffer[childIdx].position = posParent + dir * targetLength;
//    }
//}

//// -------------------------------------------------------------
//// 高精度 SDF レイマーチング（めり込み自動救出 ＆ 表面すべり移動）
//// -------------------------------------------------------------
//float3 MoveWithSDFRayMarch(float3 startPos, float3 targetPos, float hairRadius)
//{
//    float3 moveVec = targetPos - startPos;
//    float totalDist = length(moveVec);
//    if (totalDist < 0.0001f)
//        return startPos;

//    float3 currentPos = startPos;

//    // --- ★ 対策1: 開始地点のめり込み救済 ---
//    // すでに体内にいる場合は、Rayを撃つ前に法線方向（体外）へ押し出す
//    float3 startNormal;
//    float startDist = SampleSDF(currentPos, startNormal);
//    if (startDist < hairRadius)
//    {
//        currentPos += startNormal * (hairRadius - startDist + 0.001f);
//        // 救出後の位置から改めて targetPos への移動ベクトルを再計算
//        moveVec = targetPos - currentPos;
//        totalDist = length(moveVec);
//        if (totalDist < 0.0001f)
//            return currentPos;
//    }

//    float3 dir = moveVec / totalDist;
//    float traveled = 0.0f;

//    // --- ★ 対策2: 連続衝突判定 (Sphere Tracing) ---
//    for (int step = 0; step < 6; ++step)
//    {
//        float3 normal;
//        float dist = SampleSDF(currentPos, normal);
//        float safeDist = dist - hairRadius;

//        // 体の表面に接触
//        if (safeDist <= 0.001f)
//        {
//            // 残りの移動ベクトルのうち、「体内へ突っ込む成分だけ」を打ち消して表面に滑らせる
//            float remainingDist = totalDist - traveled;
//            float3 remainingVec = dir * remainingDist;

//            float dotN = dot(remainingVec, normal);
//            if (dotN < 0.0f)
//            {
//                remainingVec -= normal * dotN; // 壁に向かうベクトルを除去（スライド）
//            }

//            float3 slidePos = currentPos + remainingVec;

//            // スライド先でわずかにめり込む場合の安全マージン補正
//            float3 slideNormal;
//            float slideDist = SampleSDF(slidePos, slideNormal);
//            if (slideDist < hairRadius)
//            {
//                slidePos += slideNormal * (hairRadius - slideDist + 0.0005f);
//            }

//            return slidePos;
//        }

//        // 安全に進める距離を進む（過大なステップによるすり抜け防止のためキャップを適用）
//        float stepDist = min(safeDist, totalDist - traveled);
//        traveled += stepDist;
//        currentPos += dir * stepDist;

//        if (traveled >= totalDist)
//        {
//            return targetPos; // 障害物なしで無事到達
//        }
//    }

//    return currentPos;
//}

//[numthreads(64, 1, 1)]
//void main(uint3 dispatchThreadID : SV_DispatchThreadID)
//{
//    uint guideIdx = dispatchThreadID.x;
//    if (guideIdx >= gHairConfig.numGuides)
//        return;
    
//    GuideInfo info = g_GuideInfos[guideIdx];
//    if (info.vertexCount < 2)
//        return;

//    float dt = gHairSceneParm.gDeltaTime;
    
//    // --------------------------------------------------
//    // 事前準備: ガイドの全物理長 (totalLength) を計算する
//    // --------------------------------------------------
//    float totalLength = 0.0f;
//    for (uint k = 0; k < info.vertexCount - 1; ++k)
//    {
//        totalLength += g_GuideBuffer[info.vertexStartIndex + k].nextToLength;
//    }
    
//    // --------------------------------------------------
//    // 各頂点での風の計算
//    // --------------------------------------------------
//    float currentAccumulatedLength = 0.0f; // 根元からの累積距離

//    // 移動量（長さ）と方向を安全に計算
//    float moveLen = length(gHairMaterialParm.gMoveDirection);

//    // 停止時（moveLenがほぼゼロ）のゼロ除算を回避
//    float3 moveDir = (moveLen > 0.0001f) ? (gHairMaterialParm.gMoveDirection / moveLen) : float3(0, 0, 0);

//    // 慣性係数（効き具合の調整用パラメータ）
//    float inertiaStrength = 0.125f;

//    // スピードに応じた慣性力を設定（過大になりすぎる場合は上限を設定）
//    float3 inertia = -moveDir * min(moveLen, 0.5f) * inertiaStrength;
    
//    // =========================================================
//    // ステップ1: 外力（重力・風・慣性・復元力）の計算と位置更新
//    // =========================================================
//    for (uint i = 0; i < info.vertexCount; ++i)
//    {
//        uint currentIdx = info.vertexStartIndex + i;
//        ControllerPoint p = g_GuideBuffer[currentIdx];

//        // 根元（physicsWeight = 0）は固定のため物理計算をスキップ
//        if (p.physicsWeight <= 0.0001f)
//            continue;
        
//        // 直前の関節の長さを足して「根元から何センチか」を更新
//        // この処理の場合はここに書かないと二回分次の長さになってしまう、もしくは一個前の長さになってしまう
//        currentAccumulatedLength += g_GuideBuffer[currentIdx - 1].nextToLength;
        
//        // 根元(0.0) 〜 毛先(1.0) の相対位置を計算
//        // ( vertexCount - 1 で割ることで、毛先がぴったり 1.0 になる )
//        float normalizedDepth = (float) i / (float) (info.vertexCount - 1);

//        // 重力
//        float3 gravity = gHairSceneParm.gGravity * p.physicsWeight;

//        // 風: 根元から毛先に向かって波が伝わるように (i を使用)

//        // 正規化された位置を使って風の波（位相）を計算
//        // 【実際の距離ベースの風】
//        // 例: 1メートル（1.0）あたり 15 ラジアン進む波
//        float wavePhase = gHairSceneParm.gTime * 6.0f - currentAccumulatedLength * 15.0f;

//        // 風の力（長い髪ほど毛先の影響を強くするなどの調整も totalLength で可能）
//        float3 wind = gHairSceneParm.gWindDirection * sin(wavePhase) * p.physicsWeight;
        
//        // 復元力: 初期姿勢（homePosition）へ戻ろうとする力
//        float3 toHome = (p.homePosition - p.position) * gHairMaterialParm.gRestoringForce * (1.0f - p.physicsWeight);

//        // 外力の合算
//        float3 totalForce = gravity + wind + toHome;

//        // 速度・位置の加算（deltaTimeを反映）
//        float3 velocity = (inertia + totalForce * dt) * gHairMaterialParm.gDamping;
//        p.position += velocity;

//        g_GuideBuffer[currentIdx] = p;
//    }
    
    
//    // =========================================================
//    // PBD長さ拘束 & 衝突判定の反復ループ
//    // =========================================================
//    for (uint iteration = 0; iteration < 5; ++iteration)
//    {
//        // --- 順方向パス (根元 -> 毛先) ---
//        for (uint j = 1; j < info.vertexCount; ++j)
//        {
//            uint pIdx = info.vertexStartIndex + j - 1;
//            uint cIdx = info.vertexStartIndex + j;

//            ApplyConstraint(pIdx, cIdx);
//        }
        
//        for (uint b = 1; b < info.vertexCount - 1; ++b)
//        {
//            uint prev = info.vertexStartIndex + b - 1;
//            uint curr = info.vertexStartIndex + b;
//            uint next = info.vertexStartIndex + b + 1;
        
//        // 剛性パラメータ（0.2f〜0.4fで調整。高いほど固い髪になる）
//            ApplyBendingConstraint(prev, curr, next, 0.25f);
//        }
        
//        // SDF 
//        for (uint c = 1; c < info.vertexCount; ++c) // 根元 (c=0) は固定なので c=1 から
//        {
//            uint idx = info.vertexStartIndex + c;
//            float3 pos = g_GuideBuffer[idx].position;

//            // 髪の毛の太さ
//            float hairRadius = g_GuideBuffer[idx].radius * 1.0f;

//            if (ResolveSDFCollision(pos, hairRadius))
//            {
//                g_GuideBuffer[idx].position = pos;
//            }
//        }
     
//    }
//}

//[numthreads(64, 1, 1)]
//void main(uint3 dispatchThreadID : SV_DispatchThreadID)
//{
//    uint guideIdx = dispatchThreadID.x;
//    if (guideIdx >= gHairConfig.numGuides)
//        return;
    
//    GuideInfo info = g_GuideInfos[guideIdx];
//    if (info.vertexCount < 2)
//        return;

//    float dt = gFrameConfig.deltaTime;
    
//    float currentAccumulatedLength = 0.0f;
//    float moveLen = length(gFrameConfig.moveDirection);
//    float3 moveDir = (moveLen > 0.0001f) ? (gFrameConfig.moveDirection / moveLen) : float3(0, 0, 0);
//    float inertiaStrength = 0.125f;
//    float3 inertia = -moveDir * min(moveLen, 0.5f) * inertiaStrength;
    
//    // =========================================================
//    // ステップ1: 外力計算 ＋ RayMarchingによる事前回避
//    // =========================================================
//    for (uint i = 0; i < info.vertexCount; ++i)
//    {
//        uint currentIdx = info.vertexStartIndex + i;
//        ControllerPoint p = g_GuideBuffer[currentIdx];

//        if (p.physicsWeight <= 0.0001f)
//            continue;
        
//        currentAccumulatedLength += g_GuideBuffer[currentIdx - 1].nextToLength;

//        float wavePhase = gFrameConfig.time * 6.0f - currentAccumulatedLength * 15.0f;
//        float3 gravity = gFrameConfig.gravity * p.physicsWeight;
//        float3 wind = gFrameConfig.windDirection * sin(wavePhase) * p.physicsWeight;
//        float3 toHome = (p.homePosition - p.position) * gHairPhysicsConfig.restoringForce * (1.0f - p.physicsWeight);

//        float3 totalForce = gravity + wind + toHome;
//        float3 velocity = (inertia + totalForce * dt) * gHairPhysicsConfig.damping;
        
//        float3 targetPos = p.position + velocity;

//        // 外力で動かす段階で体内に突っ込まないようRayMarchで滑らせる
//        p.position = MoveWithSDFRayMarch(p.position, targetPos, p.radius * 0.5f);

//        g_GuideBuffer[currentIdx] = p;
//    }
    
//    // =========================================================
//    // ステップ2: FTL（長さ固定） ＋ RayMarchingによる滑らかな拘束
//    // =========================================================
//    for (uint c = 1; c < info.vertexCount; ++c)
//    {
//        uint pIdx = info.vertexStartIndex + c - 1;
//        uint cIdx = info.vertexStartIndex + c;

//        float3 parentPos = g_GuideBuffer[pIdx].position;
//        float3 childPos = g_GuideBuffer[cIdx].position;
//        float targetLen = g_GuideBuffer[pIdx].nextToLength;
//        float hairRadius = g_GuideBuffer[cIdx].radius * 0.5f;

//        // 1. 親を中心とする球面上（長さ targetLen）の目標地点をセット
//        float3 delta = childPos - parentPos;
//        float dist = length(delta);
//        float3 dir = (dist > 0.0001f) ? (delta / dist) : float3(0, -1, 0);
//        float3 targetPos = parentPos + dir * targetLen;

//        // 2. 目標地点に向かってRayMarch（衝突したら体表を滑る）
//        float3 nextPos = MoveWithSDFRayMarch(childPos, targetPos, hairRadius);

//        // 3. 親との距離を絶対固定（伸び率0%化）
//        float3 finalDelta = nextPos - parentPos;
//        float finalDist = length(finalDelta);
//        if (finalDist > 0.0001f)
//        {
//            nextPos = parentPos + (finalDelta / finalDist) * targetLen;
//        }

//        // 4. 厳密な長さ固定で万が一体内に僅かに再進入した時の最終セーフティ
//        float3 finalNormal;
//        float finalSdf = SampleSDF(nextPos, finalNormal);
//        if (finalSdf < hairRadius)
//        {
//            nextPos += finalNormal * (hairRadius - finalSdf + 0.0005f);
//        }

//        g_GuideBuffer[cIdx].position = nextPos;
//    }
//}

//[numthreads(64, 1, 1)]
//void main(uint3 dispatchThreadID : SV_DispatchThreadID)
//{
//    uint guideIdx = dispatchThreadID.x;
//    if (guideIdx >= gHairConfig.numGuides)
//        return;
    
//    GuideInfo info = g_GuideInfos[guideIdx];
//    if (info.vertexCount < 2)
//        return;

//    float dt = gFrameConfig.deltaTime;
    
//    float currentAccumulatedLength = 0.0f;
//    float moveLen = length(gFrameConfig.moveDirection);
//    float3 moveDir = (moveLen > 0.0001f) ? (gFrameConfig.moveDirection / moveLen) : float3(0, 0, 0);
//    float inertiaStrength = 0.125f;
//    float3 inertia = -moveDir * min(moveLen, 0.5f) * inertiaStrength;
    
//    // =========================================================
//    // ステップ1: 外力計算（★体表付近での体内押し込み力を事前に殺す）
//    // =========================================================
//    for (uint i = 0; i < info.vertexCount; ++i)
//    {
//        uint currentIdx = info.vertexStartIndex + i;
//        ControllerPoint p = g_GuideBuffer[currentIdx];

//        if (p.physicsWeight <= 0.0001f)
//            continue;
        
//        currentAccumulatedLength += g_GuideBuffer[currentIdx - 1].nextToLength;

//        float wavePhase = gFrameConfig.time * 6.0f - currentAccumulatedLength * 15.0f;
//        float3 gravity = gFrameConfig.gravity * p.physicsWeight;
//        float3 wind = gFrameConfig.windDirection * sin(wavePhase) * p.physicsWeight;
//        float3 toHome = (p.homePosition - p.position) * gHairPhysicsConfig.restoringForce * (1.0f - p.physicsWeight);

//        float3 totalForce = gravity + wind + toHome;

//        // ★ 対策1: 体表付近（マージン0.005m）にいる場合、体内へ向かう力をゼロにして滑らせる
//        float3 sdfNormal;
//        float sdfDist = SampleSDF(p.position, sdfNormal);
//        float hairRadius = p.radius * 0.5f;

//        if (sdfDist < hairRadius + 0.005f)
//        {
//            float forceDot = dot(totalForce, sdfNormal);
//            if (forceDot < 0.0f)
//            {
//                totalForce -= sdfNormal * forceDot; // 体内へ向かう力だけ除去
//            }
//        }

//        float3 velocity = (inertia + totalForce * dt) * gHairPhysicsConfig.damping;
//        p.position += velocity;

//        g_GuideBuffer[currentIdx] = p;
//    }
    
//    // =========================================================
//    // ステップ2: FTL ＋ カプセル（線分）SDF衝突回避（★微小マージン付き）
//    // =========================================================
//    for (uint c = 1; c < info.vertexCount; ++c)
//    {
//        uint pIdx = info.vertexStartIndex + c - 1;
//        uint cIdx = info.vertexStartIndex + c;

//        float3 parentPos = g_GuideBuffer[pIdx].position;
//        float3 childPos = g_GuideBuffer[cIdx].position;
//        float targetLen = g_GuideBuffer[pIdx].nextToLength;
//        float hairRadius = g_GuideBuffer[cIdx].radius * 0.5f;

//        // ① まず親を中心とする正確な長さ（targetLen）の位置に置く
//        float3 delta = childPos - parentPos;
//        float dist = length(delta);
//        float3 dir = (dist > 0.0001f) ? (delta / dist) : float3(0, -1, 0);
//        childPos = parentPos + dir * targetLen;

//        // ② カプセルSDF判定
//        const float sampleT[2] = { 0.5f, 1.0f };

//        for (int s = 0; s < 2; ++s)
//        {
//            float t = sampleT[s];
//            float3 samplePos = lerp(parentPos, childPos, t);

//            float3 normal;
//            float sdfDist = SampleSDF(samplePos, normal);
//            float penetration = hairRadius - sdfDist;

//            // ★ 対策2: わずかに境界へ浮かせる安全マージン（0.0005m）を追加
//            if (penetration > -0.0005f)
//            {
//                float pushAmount = penetration + 0.0005f;
//                float pushFactor = 1.0f / max(t, 0.2f);
                
//                childPos += normal * (pushAmount * pushFactor);

//                // 押し出し後の長さ再投影
//                float3 newDelta = childPos - parentPos;
//                float newDist = length(newDelta);
//                if (newDist > 0.0001f)
//                {
//                    childPos = parentPos + (newDelta / newDist) * targetLen;
//                }
//            }
//        }

//        g_GuideBuffer[cIdx].position = childPos;
//    }
//}