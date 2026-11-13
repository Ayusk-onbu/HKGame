#ifndef SDF_FUNCTION_HLSLI
#define SDF_FUNCTION_HLSLI

struct SDFFunctionStackNodeGPU
{
    float32_t4x4 localNodeInverse;
    float32_t3 params;
    uint32_t nodeType;
    float32_t smoothK;
    float32_t3 padding;
};

struct SDFFunctionInstanceGPU
{
    float32_t4x4 worldInverse;
    float32_t3 aabbMin;
    uint32_t nodeStartOffset;
    float32_t3 aabbMax;
    uint32_t nodeCount;
};

// =============================
// 【 SDF Primitive Functions 】
// =============================
namespace SDFFunctionPrimitive
{
    float32_t Sphere(float32_t3 p, float32_t r)
    {
        return length(p) - r;
    }

    float32_t Box(float32_t3 p, float32_t3 b)
    {
        float32_t3 q = abs(p) - b;
        return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
    }

    float32_t Cylinder(float32_t3 p, float32_t2 h)
    {
        float32_t2 d = abs(float32_t2(length(p.xz), p.y)) - h;
        return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
    }

    float32_t Torus(float32_t3 p, float32_t2 t)
    {
        float32_t2 q = float32_t2(length(p.xz) - t.x, p.y);
        return length(q) - t.y;
    }
}

// ===================
// 【 CSG Functions 】
// ===================
namespace SDFFunctionOperator
{
    float32_t SmoothUnion(float32_t d1, float32_t d2, float32_t k)
    {
        float32_t h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
        return lerp(d2, d1, h) - k * h * (1.0 - h);
    }

    float32_t SmoothSubtract(float32_t d1, float32_t d2, float32_t k)
    {
        float32_t h = clamp(0.5 - 0.5 * (d2 + d1) / k, 0.0, 1.0);
        return lerp(d1, -d2, h) + k * h * (1.0 - h);
    }
}

// ===================
// 【 SDF Evaluator 】
// ===================
struct SDFEvaluator
{
    StructuredBuffer<SDFFunctionInstanceGPU> instances;
    StructuredBuffer<SDFFunctionStackNodeGPU> nodes;

    // AABB との概算距離（早期カリング用）
    float32_t BoxBoundDistance(float32_t3 p, float32_t3 bMin, float32_t3 bMax)
    {
        float32_t3 center = (bMin + bMax) * 0.5;
        float32_t3 extent = (bMax - bMin) * 0.5;
        return SDFFunctionPrimitive::Box(p - center, extent);
    }

    // 指定した1インスタンスに対する正確な SDF 距離を評価
    float32_t EvaluateInstance(uint32_t instanceIdx, float32_t3 worldPos)
    {
        SDFFunctionInstanceGPU inst = instances[instanceIdx];
        float32_t3 localPos = mul(float32_t4(worldPos, 1.0),inst.worldInverse).xyz;

        // 【高速化】インスタンスの AABB から一定以上離れていたら詳細なスタック計算をスキップ
        // ※正確な勾配(法線)が必要な場合や、スケールが大きい場合はマージンを調整する
        float32_t boundDist = BoxBoundDistance(localPos, inst.aabbMin, inst.aabbMax);
        if (boundDist > 2.0)
        {
            return boundDist;
        }

        float32_t stack[8];
        int32_t stackPtr = -1;

        for (uint32_t i = 0; i < inst.nodeCount; ++i)
        {
            SDFFunctionStackNodeGPU node = nodes[inst.nodeStartOffset + i];

            if (node.nodeType < 100)
            {
                float32_t3 nodePos = mul(float32_t4(localPos, 1.0),node.localNodeInverse).xyz;
                float32_t d = 1e5;

                switch (node.nodeType)
                {
                    case 0:
                        d = SDFFunctionPrimitive::Sphere(nodePos, node.params.x);
                        break;
                    case 1:
                        d = SDFFunctionPrimitive::Box(nodePos, node.params);
                        break;
                    case 2:
                        d = SDFFunctionPrimitive::Cylinder(nodePos, node.params.xy);
                        break;
                    case 3:
                        d = SDFFunctionPrimitive::Torus(nodePos, node.params.xy);
                        break;
                }

                stack[++stackPtr] = d;
            }
            else
            {
                // 演算ノード
                float32_t b = stack[stackPtr--];
                float32_t a = stack[stackPtr--];
                float32_t res = a;

                switch (node.nodeType)
                {
                    case 100:
                        res = min(a, b);
                        break; // Union
                    case 101:
                        res = max(a, -b);
                        break; // Subtract
                    case 102:
                        res = max(a, b);
                        break; // Intersection
                    case 103:
                        res = SDFFunctionOperator::SmoothUnion(a, b, node.smoothK);
                        break; // SmoothUnion
                    case 104:
                        res = SDFFunctionOperator::SmoothSubtract(a, b, node.smoothK);
                        break; // SmoothSubtract
                }

                stack[++stackPtr] = res;
            }
        }

        return (stackPtr >= 0) ? stack[0] : 1e5;
    }

    // シーン内の全 SDF インスタンスとの最短距離を一括評価
    float32_t EvaluateScene(float32_t3 worldPos, uint32_t instanceCount, out int32_t outHitInstanceIdx)
    {
        float32_t minDist = 1e5;
        outHitInstanceIdx = -1;

        for (uint32_t idx = 0; idx < instanceCount; ++idx)
        {
            float32_t d = EvaluateInstance(idx, worldPos);
            if (d < minDist)
            {
                minDist = d;
                outHitInstanceIdx = (int32_t) idx;
            }
        }
        return minDist;
    }
};

#endif// SDF_FUNCTION_HLSLI