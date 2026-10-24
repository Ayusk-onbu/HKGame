#include "Hair.hlsli"

StructuredBuffer<ControllerPoint> g_InGuideBuffer : register(t0); // 物理演算後のガイド頂点群
StructuredBuffer<ChildStrand> g_InChildStrandBuffer : register(t1); // 子髪の設定バッファ
StructuredBuffer<StrandInfo> g_StrandInfoBuffer : register(t2);
StructuredBuffer<GuideInfo> g_GuideInfoBuffer : register(t3);
RWStructuredBuffer<StrandVertex> g_OutVertexBuffer : register(u0);
ConstantBuffer<HairMaterialParameter> g_HairMaterialPara : register(b0);
ConstantBuffer<HairConfig> gHairConfig : register(b1);

float3 CatmullRom(float3 p0, float3 p1, float3 p2, float3 p3, float t)
{
    float t2 = t * t;
    float t3 = t2 * t;

    return 0.5 * (
        (2.0 * p1) +
        (-p0 + p2) * t +
        (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 +
        (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3
    );
}

// Catmull-Romの微分による接線（Tangent）の計算
float3 CatmullRomDerivative(float3 p0, float3 p1, float3 p2, float3 p3, float t)
{
    float t2 = t * t;

    return 0.5 * (
        (-p0 + p2) +
        2.0 * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t +
        3.0 * (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t2
    );
}

[numthreads(64, 1, 1)] // 64本ずつ並列で一気に生やす！
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    // 担当する髪のＩＤを取得
    uint strandIdx = dispatchThreadID.x;
    // 総ストランド数
    if (strandIdx >= gHairConfig.numStrands)return;
    
    // -------------------------------------------
    
    // この子髪の遺伝子（設定）をロード
    ChildStrand child = g_InChildStrandBuffer[strandIdx];
    StrandInfo info = g_StrandInfoBuffer[strandIdx];
    float3 currentNormal = float3(0.0, 0.0, 0.0);
    
    // -------------------------------------------------------------
    // 根元から毛先まで、1本分の全頂点を生やすループ
    // -------------------------------------------------------------
    for (uint i = 0; i < info.vertexCount; ++i)
    {
        // ストランド全体での進行割合 (0.0 ～ 1.0)
        float progress = (float) i / (float) (info.vertexCount - 1);

        float3 blendedPos = float3(0.0, 0.0, 0.0);
        float3 blendedTangent = float3(0.0, 0.0, 0.0);
        float blendedRadius = 0.0;
        float3 blendedColor = float3(0.0, 0.0, 0.0);

        uint numBlend = (child.blendMode == 1) ? 3 : 1;

        for (uint b = 0; b < numBlend; ++b)
        {
            if (child.weights[b] <= 0.0)
                continue;

            // 各ガイドの情報をちゃんと取得する
            uint guideId = child.parentGuideIds[b];
            GuideInfo gInfo = g_GuideInfoBuffer[guideId];
            
            // 掛け算ではなくメンバから直接StartIndexを取る！
            uint guideStartIndex = gInfo.vertexStartIndex;

            // このガイド自身の頂点数に合わせて、インデックスと t を計算し直す
            float gFloatIdx = progress * (float) (gInfo.vertexCount - 1);
            int idx1 = (int) floor(gFloatIdx);
            int idx2 = min(idx1 + 1, (int) gInfo.vertexCount - 1);
            int idx0 = max(idx1 - 1, 0);
            int idx3 = min(idx2 + 1, (int) gInfo.vertexCount - 1);
            float t = gFloatIdx - (float) idx1;

            // これで絶対に範囲外を引かない安全なインデックスになる！
            ControllerPoint gPoint0 = g_InGuideBuffer[guideStartIndex + idx0];
            ControllerPoint gPoint1 = g_InGuideBuffer[guideStartIndex + idx1];
            ControllerPoint gPoint2 = g_InGuideBuffer[guideStartIndex + idx2];
            ControllerPoint gPoint3 = g_InGuideBuffer[guideStartIndex + idx3];

            // ガイド単体の計算
            float3 pos = CatmullRom(gPoint0.position, gPoint1.position, gPoint2.position, gPoint3.position, t);
            float3 tan = CatmullRomDerivative(gPoint0.position, gPoint1.position, gPoint2.position, gPoint3.position, t);
            float rad = lerp(gPoint1.radius, gPoint2.radius, t);
            float3 col = lerp(gPoint1.color, gPoint2.color, t);

            // ウェイトを掛けて合算
            float weight = child.weights[b];
            blendedPos += pos * weight;
            blendedTangent += tan * weight;
            blendedRadius += rad * weight;
            blendedColor += col * weight;
        }

        float3 currentPos = blendedPos;
        // 接線は合成後に再度正規化する
        float3 tangent = normalize(blendedTangent);
        float currentRadius = blendedRadius;
        float3 currentColor = blendedColor;
        
        // 髪がねじれてがくがくしてしまうのを防ぐため、法線ベクトルを前の頂点からスライドさせて計算する
        if (i == 0)
        {
            // 根元（最初の頂点）のときだけ、最初の基準を作る
            float3 arbitraryUp = float3(0.0, 1.0, 0.0);
            if (abs(dot(tangent, arbitraryUp)) > 0.95)
            {
                arbitraryUp = float3(1.0, 0.0, 0.0);
            }
            currentNormal = normalize(cross(tangent, arbitraryUp));
        }
        else
        {
            // 2手目以降（毛先まで）は「前の頂点の法線」をベースに変形させる！
            // 現在の接線(tangent)に対して垂直になるように、前の法線をスライド（射影）させる
            currentNormal = normalize(currentNormal - tangent * dot(currentNormal, tangent));
        }
        
        float3 normal = currentNormal;
        float3 binormal = cross(tangent, normal);

        float3 offset3D = (normal * child.offset.x) + (binormal * child.offset.y);
        
        float clump = lerp(1.0, (1.0 - child.clumpForce), progress);
        offset3D *= clump;

        // 最終座標
        StrandVertex outVertex;
        outVertex.position = currentPos + offset3D;

        float tipTaper = 1.0 - (progress * 0.5);
        outVertex.radius = currentRadius * tipTaper * g_HairMaterialPara.gGlobalHairThickness;

        outVertex.color = currentColor;
        outVertex.padding = 0.0;

        g_OutVertexBuffer[info.vertexStartIndex + i] = outVertex;
    }
}