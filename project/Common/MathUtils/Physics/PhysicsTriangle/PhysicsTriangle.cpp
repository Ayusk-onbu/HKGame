#include "PhysicsTriangle.h"

AABB PhysicsTriangle::GetAABB()const {
    AABB bounds;
    bounds.ResetToExtreme(); // 極小状態からスタート
    bounds.Expand(v0);
    bounds.Expand(v1);
    bounds.Expand(v2);
    return bounds;
}

Vector3 PhysicsTriangle::GetCenter()const {
    return{
        (v0.x + v1.x + v2.x) / 3.0f,
        (v0.y + v1.y + v2.y) / 3.0f,
        (v0.z + v1.z + v2.z) / 3.0f
    };
}