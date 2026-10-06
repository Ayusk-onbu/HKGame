#include "Particle.hlsli"
#include "../Common.hlsli"

struct SDFInfo
{
    float3 gridMin; // SDFバウンディングボックスの最小点
    float pad0;
    float3 gridMax; // SDFバウンディングボックスの最大点
    float pad1;
    uint3 resolution; // テクスチャ解像度 (例: 64, 64, 64)
    uint totalNodes;
};

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<Emitter> gEmitter : register(b0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<SDFInfo> g_SDFConfig : register(b2);

Texture3D<float> g_SDFTexture : register(t0);
SamplerState g_LinearSampler : register(s0);

// SDFから表面への補正座標と法線を求める関数
void ProjectToSDFSurface(inout float3 position, out float3 normal)
{
    float3 gridExtent = g_SDFConfig.gridMax - g_SDFConfig.gridMin;
    float3 uvw = (position - g_SDFConfig.gridMin) / gridExtent;

    // 範囲外ならデフォルトの法線を返して終了
    if (any(uvw < 0.0f) || any(uvw > 1.0f))
    {
        normal = float3(0, 1, 0);
        return;
    }

    // 距離の取得
    float dist = g_SDFTexture.SampleLevel(g_LinearSampler, uvw, 0);

    // 1ヴォクセル幅で勾配（法線）を計算
    float3 delta = 1.0f / (float3) g_SDFConfig.resolution;
    float dX1 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw + float3(delta.x, 0, 0), 0);
    float dX2 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw - float3(delta.x, 0, 0), 0);
    float dY1 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw + float3(0, delta.y, 0), 0);
    float dY2 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw - float3(0, delta.y, 0), 0);
    float dZ1 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw + float3(0, 0, delta.z), 0);
    float dZ2 = g_SDFTexture.SampleLevel(g_LinearSampler, uvw - float3(0, 0, delta.z), 0);

    float3 gradient = float3((dX1 - dX2) / gridExtent.x, (dY1 - dY2) / gridExtent.y, (dZ1 - dZ2) / gridExtent.z);
    normal = normalize(gradient);

    if (length(gradient) < 0.0001f)
        normal = float3(0, 1, 0);

    // 距離 dist 分だけ法線方向に引き戻す/押し出すことで「表面」に配置
    position -= normal * dist;
}

[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (gEmitter.emit != 0)
    {
        RandomGenerator generator;
        generator.seed = (dispatchThreadID + gPerFrame.time) * gPerFrame.time;

        for (uint32_t countIndex = 0; countIndex < gEmitter.count; ++countIndex)
        {
            int32_t freeListIndex;
            InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);

            if (0 <= freeListIndex && freeListIndex < 1024)
            {
                uint32_t particleIndex = gFreeList[freeListIndex];

                // SDFのAABB内のランダム座標を生成
                float3 randomLocalPos = lerp(g_SDFConfig.gridMin, g_SDFConfig.gridMax, generator.Generate3d());

                // ランダム座標をSDFの表面にフィットさせ、その場の法線を取得
                float3 surfaceNormal;
                ProjectToSDFSurface(randomLocalPos, surfaceNormal);

                // パーティクル初期化
                gParticles[particleIndex].translate = randomLocalPos; // 表面位置を設定
                gParticles[particleIndex].scale = float3(0.05f, 0.05f, 0.05f);
                gParticles[particleIndex].color = float4(generator.Generate3d(), 1.0f);
                gParticles[particleIndex].lifeTime = 10.0f;
                gParticles[particleIndex].currentTime = 0.0f;

                // 法線方向に吹き飛ぶような初期速度（オーラや発光エフェクトに最適）
                gParticles[particleIndex].velocity = float3(0.0f, 0.0f, 0.0f);
            }
            else
            {
                InterlockedAdd(gFreeListIndex[0], 1);
                break;
            }
        }
    }
}