#include "SDFFunctionManager.h"

void SDFFunctionManager::Initialize(Fngine* fngine) {
    pFngine_ = fngine;
    activeInstanceCount_ = 0;

    // 起動直後から有効な SRV ハンドルを持たせるため、最小サイズのダミーバッファを確保
    EnsureCapacity(1, 1);

    // ダミー領域をゼロクリア
    if (instanceBuffer_ && instanceBuffer_->GetMappedData()) {
        std::memset(instanceBuffer_->GetMappedData(), 0, sizeof(SDFFunctionStackInstanceGPU));
    }
    if (nodeBuffer_ && nodeBuffer_->GetMappedData()) {
        std::memset(nodeBuffer_->GetMappedData(), 0, sizeof(SDFFunctionStackNodeGPU));
    }
}

void SDFFunctionManager::UpdateGPUBuffer(const std::vector<SDFComposedResult>& results) {
    if (results.empty()) {
        activeInstanceCount_ = 0;
        return;
    }

    uint32_t totalInstances = static_cast<uint32_t>(results.size());
    uint32_t totalNodes = 0;
    for (const auto& res : results) {
        totalNodes += static_cast<uint32_t>(res.nodes.size());
    }

    // インスタンス数または総ノード数が 0 ならバッファ更新を行わない
    if (totalInstances == 0 || totalNodes == 0) {
        activeInstanceCount_ = 0;
        return;
    }

    EnsureCapacity(totalInstances, totalNodes);

    SDFFunctionStackInstanceGPU* mappedInstances = instanceBuffer_->GetMappedData();
    SDFFunctionStackNodeGPU* mappedNodes = nodeBuffer_->GetMappedData();

    uint32_t currentNodeOffset = 0;

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& res = results[i];
        uint32_t nodeCount = static_cast<uint32_t>(res.nodes.size());

        SDFFunctionStackInstanceGPU instGPU{};
        instGPU.worldInverse = Matrix4x4::Inverse(res.worldMatrix);
        instGPU.aabbMin = res.aabbMin;
        instGPU.aabbMax = res.aabbMax;
        instGPU.nodeStartOffset = currentNodeOffset;
        instGPU.nodeCount = nodeCount;
        mappedInstances[i] = instGPU;

        for (size_t n = 0; n < nodeCount; ++n) {
            const auto& node = res.nodes[n];
            SDFFunctionStackNodeGPU nodeGPU{};
            nodeGPU.nodeType = static_cast<uint32_t>(node.type);
            nodeGPU.params = node.params;
            nodeGPU.smoothK = node.smoothK;

            if (nodeGPU.nodeType < 100) {
                Vector3 safeScale = {
                    (std::max)(std::abs(node.scale.x), 0.001f),
                    (std::max)(std::abs(node.scale.y), 0.001f),
                    (std::max)(std::abs(node.scale.z), 0.001f)
                };
                Matrix4x4 localNodeMat = Matrix4x4::Make::Affine(safeScale, node.rotation, node.position);

                if (node.pFollowMatrix != nullptr) {
                    Matrix4x4 followMat = *node.pFollowMatrix;
                    // ボーン行列からスケール成分（0.01や100など）を除去し、回転と平行移動のみ抽出
                    for (int row = 0; row < 3; ++row) {
                        float len = std::sqrt(
                            followMat.m[row][0] * followMat.m[row][0] +
                            followMat.m[row][1] * followMat.m[row][1] +
                            followMat.m[row][2] * followMat.m[row][2]
                        );
                        if (len > 1e-6f) {
                            followMat.m[row][0] /= len;
                            followMat.m[row][1] /= len;
                            followMat.m[row][2] /= len;
                        }
                    }
                    localNodeMat = localNodeMat * followMat;
                }

                nodeGPU.localToNodeInverse = Matrix4x4::Inverse(localNodeMat);
            }
            else {
                nodeGPU.localToNodeInverse = Matrix4x4::Make::Identity();
            }

            mappedNodes[currentNodeOffset + n] = nodeGPU;
        }

        currentNodeOffset += nodeCount;
    }

    activeInstanceCount_ = totalInstances;
}

void SDFFunctionManager::EnsureCapacity(uint32_t instanceCount, uint32_t nodeCount) {
    if (!instanceBuffer_ || instanceCount > maxInstanceCapacity_) {
        maxInstanceCapacity_ = (instanceCount + 31) & ~31;
        instanceBuffer_ = std::make_unique<Structured<SDFFunctionStackInstanceGPU>>(pFngine_);
        instanceBuffer_->Initialize(maxInstanceCapacity_);
    }
    if (!nodeBuffer_ || nodeCount > maxNodeCapacity_) {
        maxNodeCapacity_ = (nodeCount + 63) & ~63;
        nodeBuffer_ = std::make_unique<Structured<SDFFunctionStackNodeGPU>>(pFngine_);
        nodeBuffer_->Initialize(maxNodeCapacity_);
    }
}