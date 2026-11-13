#pragma once
#include "ISingleton.h"
#include "SDFFunctionComponent.h"
#include <unordered_set>
#include <vector>

// 評価済みの1オブジェクト分データ
struct SDFComposedResult {
    Matrix4x4 worldMatrix = Matrix4x4::Make::Identity();
    Vector3 aabbMin = { 0.0f, 0.0f, 0.0f };
    Vector3 aabbMax = { 0.0f, 0.0f, 0.0f };
    std::vector<SDFFunctionStackNodeCPU> nodes;
};

class SDFSystem : public ISingleton<SDFSystem> {
    friend class ISingleton<SDFSystem>;
public:
    // コンポーネントからの自動登録・解除 API
    void Register(SDFFunctionComponent* comp) {
        if (comp) activeComponents_.insert(comp);
    }
    void Unregister(SDFFunctionComponent* comp) {
        activeComponents_.erase(comp);
    }

    // 全 SDFFunctionComponent を評価して描画用パッケージ群を生成
    std::vector<SDFComposedResult> EvaluateAll();

private:
    std::unordered_set<SDFFunctionComponent*> activeComponents_;
};