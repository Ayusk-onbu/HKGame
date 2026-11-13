#include "SDFFunctionSystem.h"
#include "DynamicObject.h"

std::vector<SDFComposedResult> SDFSystem::EvaluateAll() {
    std::vector<SDFComposedResult> results;
    results.reserve(activeComponents_.size());

    for (auto* comp : activeComponents_) {
        if (!comp || !comp->master_ || comp->GetNodes().empty()) continue;

        comp->ResolveBoneBindings();
        SDFComposedResult res{};
        res.worldMatrix = comp->master_->GetTransform().mat_;
        // アニメーション中のボーン位置を反映した最新の AABB を取得
        comp->GetCurrentLocalAABB(res.aabbMin, res.aabbMax);
        res.nodes = comp->GetNodes();

        // ※将来「Component同士の計算」を追加したくなったら、
        //   この EvaluateAll 内で activeComponents_ 同士の組み合わせ処理を記述する

        results.push_back(res);
    }

    return results;
}
