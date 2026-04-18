#pragma once
#include "Structures.h"
#include "AABB.h"

struct PhysicsTriangle {
	// 頂点
	Vector3 v0, v1, v2;
	// 元の配列のインデックス
	int originalIndex;

	// TriangleにぴったりのAABBを作成
	AABB GetAABB()const;

	// Triangleの中心（重心）を取得
	Vector3 GetCenter()const;
};