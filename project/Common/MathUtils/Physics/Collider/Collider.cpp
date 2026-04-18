#include "Collider.h"


/// ====================
/// 
/// __ Mesh Collider __
/// 
/// ====================
void MeshCollider::UpdateAABB() {
    // 頂点が1つもセットされていない場合は計算しない
    if (vertices_.empty()) {
        return;
    }

    // 最初は、0番目の頂点を最小値・最大値の基準にする
    Vector3 minLocal = vertices_[0];
    Vector3 maxLocal = vertices_[0];

    // 1番目以降の頂点と比較して、最小・最大を更新していく
    for (size_t i = 1; i < vertices_.size(); ++i) {
        minLocal.x = std::min(minLocal.x, vertices_[i].x);
        minLocal.y = std::min(minLocal.y, vertices_[i].y);
        minLocal.z = std::min(minLocal.z, vertices_[i].z);

        maxLocal.x = std::max(maxLocal.x, vertices_[i].x);
        maxLocal.y = std::max(maxLocal.y, vertices_[i].y);
        maxLocal.z = std::max(maxLocal.z, vertices_[i].z);
    }

    // 計算したローカルの最小値・最大値に、ワールド座標を足して aabb_ にセットする
    // ※ AABB構造体が min, max というメンバを持っている想定です
    aabb_.min.x = minLocal.x + worldPosition_.x;
    aabb_.min.y = minLocal.y + worldPosition_.y;
    aabb_.min.z = minLocal.z + worldPosition_.z;

    aabb_.max.x = maxLocal.x + worldPosition_.x;
    aabb_.max.y = maxLocal.y + worldPosition_.y;
    aabb_.max.z = maxLocal.z + worldPosition_.z;
}

void MeshCollider::Update() {
    worldVertices_.clear();
    for (const auto& v : vertices_) {
        Vector4 worldPos = Matrix4x4::Transform(*worldMatrix_, Vector4{ v.x, v.y, v.z, 1.0f });
        // ローカル座標にワールド座標を足して、Vector2に変換
        worldVertices_.emplace_back(worldPos.x, worldPos.y, worldPos.z);
    }
}