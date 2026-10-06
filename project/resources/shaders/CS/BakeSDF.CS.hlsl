struct GPUBVHNode
{
    float3 minBounds;
    int leftChildIndex; // -1 なら葉ノード (Leaf)

    float3 maxBounds;
    int rightChildIndex; // 葉ノード時は polygonStartIndex

    int polygonCount; // 0 なら内部ノード
    int pad0;
    int pad1;
    int pad2;
};

struct GPUPolygon
{
    float3 v0;
    float pad0;
    float3 v1;
    float pad1;
    float3 v2;
    float pad2;
};

struct SDFBakeConfig
{
    float3 gridMin; // SDFバウンディングボックスの最小点
    float pad0;
    float3 gridMax; // SDFバウンディングボックスの最大点
    float pad1;
    uint3 resolution; // テクスチャ解像度 (例: 64, 64, 64)
    uint totalNodes;
};

// バッファリソース
StructuredBuffer<GPUBVHNode> g_BVHNodes : register(t0);
StructuredBuffer<GPUPolygon> g_Polygons : register(t1);
RWTexture3D<float> g_SDFOutput : register(u0);

ConstantBuffer<SDFBakeConfig> g_Config : register(b0);

// -------------------------------------------------------------
// 点 P と AABB の最短距離の「2乗」を計算する（枝刈り用）
// -------------------------------------------------------------
float PointAABBDistanceSq(float3 p, float3 bMin, float3 bMax)
{
    float3 cl = clamp(p, bMin, bMax);
    float3 d = p - cl;
    return dot(d, d);
}

// -------------------------------------------------------------
// 点 P に最も近い三角形上の点 (Closest Point) を求める
// -------------------------------------------------------------
float3 ClosestPointOnTriangle(float3 p, float3 a, float3 b, float3 c)
{
    float3 ab = b - a;
    float3 ac = c - a;
    float3 ap = p - a;

    float d1 = dot(ab, ap);
    float d2 = dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f)
        return a;

    float3 bp = p - b;
    float d3 = dot(ab, bp);
    float d4 = dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3)
        return b;

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
    {
        float v = d1 / (d1 - d3);
        return a + v * ab;
    }

    float3 cp = p - c;
    float d5 = dot(ab, cp);
    float d6 = dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6)
        return c;

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
    {
        float w = d2 / (d2 - d6);
        return a + w * ac;
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
    {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + w * (c - b);
    }

    float denom = 1.0f / (va + vb + vc);
    float v_val = vb * denom;
    float w_val = vc * denom;
    return a + ab * v_val + ac * w_val;
}

// -------------------------------------------------------------
// Compute Shader Main
// -------------------------------------------------------------
[numthreads(8, 8, 8)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    // グリッド範囲外のアクセス防止
    if (any(dispatchThreadID >= g_Config.resolution))
        return;

    //　インデックス (x, y, z) から 3D空間のワールド座標 P を計算
    float3 uvw = ((float3) dispatchThreadID + 0.5f) / (float3) g_Config.resolution;
    float3 P = lerp(g_Config.gridMin, g_Config.gridMax, uvw);

    // BVH探索用の初期化
    float minDistanceSq = 1e10f; // 十分に大きい値
    float3 closestNormal = float3(0, 1, 0);
    float3 closestPoint = P;

    // ローカルスタック
    static const int stackSize = 64; // 最大スタックサイズ
    int stack[stackSize];// 探索可能深度
    int stackPtr = 0;
    stack[stackPtr++] = 0; // ルートノード (Index: 0) から開始

    // GPU上での BVH 巡回ループ
    while (stackPtr > 0)
    {
        int nodeIdx = stack[--stackPtr];
        GPUBVHNode node = g_BVHNodes[nodeIdx];

        // 枝刈り（Pruning）: このAABBへの最短距離が、現在の暫定最短距離より遠ければスキップ！
        if (PointAABBDistanceSq(P, node.minBounds, node.maxBounds) >= minDistanceSq)
            continue;

        // 【葉ノードの場合】 ポリゴンと距離判定
        if (node.leftChildIndex == -1)
        {
            int startPoly = node.rightChildIndex;
            int count = node.polygonCount;

            for (int i = 0; i < count; ++i)
            {
                GPUPolygon poly = g_Polygons[startPoly + i];
                
                // 点 P から三角形上の最寄点を計算
                float3 cp = ClosestPointOnTriangle(P, poly.v0, poly.v1, poly.v2);
                float3 delta = P - cp;
                float distSq = dot(delta, delta);

                // より近い三角形が見つかったら更新
                if (distSq < minDistanceSq)
                {
                    minDistanceSq = distSq;
                    closestPoint = cp;

                    // 面法線を計算（内外判定用）
                    float3 n = cross(poly.v1 - poly.v0, poly.v2 - poly.v0);
                    closestNormal = (length(n) > 0.0001f) ? normalize(n) : float3(0, 1, 0);
                }
            }
        }
        // 【内部ノードの場合】 子ノードをスタックに追加
        else
        {
            if (node.leftChildIndex != -1 && stackPtr < stackSize)
                stack[stackPtr++] = node.leftChildIndex;
            if (node.rightChildIndex != -1 && stackPtr < stackSize)
                stack[stackPtr++] = node.rightChildIndex;
        }
    }

    // 外側 / 内側 の符号（Sign）判定
    float distanceVal = sqrt(minDistanceSq);
    float3 dirToPoint = P - closestPoint;
    
    // 面の法線と同じ側なら「外側（＋）」、逆側なら「内側（ー）」
    if (dot(dirToPoint, closestNormal) < 0.0f)
    {
        distanceVal = -distanceVal;
    }

    // 3D Volume Texture に書き込み
    g_SDFOutput[dispatchThreadID] = distanceVal;
}