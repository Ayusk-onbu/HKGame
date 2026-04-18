#pragma once
#include "AABB.h"
#include "PhysicsTriangle.h"

#include <memory>
#include <vector>

// ================================================
// 【 BVH 】 とは
// 名称：Bounding Volume Hierarchy
// 読み方：バウンディング・ボリューム・ヒエラルキー
// 意味：境界ボリューム階層
// ================================================

struct BVHNode {
	AABB bounds;
	// 自身を分けたときの左側
	std::unique_ptr<BVHNode> left = nullptr;
	// 自身を分けたときの右側
	std::unique_ptr<BVHNode> right = nullptr;
	// 自身が葉ノードの場合、含まれるIndexを登録
	std::vector<int> triangleIndices;
	// 自身が葉ノードかどうか
	bool IsLeaf()const {
		return left == nullptr && right == nullptr;
	}
};

// BVHを管理する
class BVH {
public:
	// 三角形(ポリゴン)のリストからツリーを構築
	void Build(std::vector<PhysicsTriangle>& triangles);
	
	// ルートノードの取得
	const BVHNode* GetRoot()const { return root_.get();}
	
	/// <summary>
	/// 設定したAABBと当たっていたらその中にある三角形のIndexを返す
	/// </summary>
	/// <param name="node"></param>
	/// <param name="targetAABB">：当たり判定を取りたい物体のAABB</param>
	/// <param name="outIndices">当たり判定に使う三角形のIndexの情報</param>
	void CollectPotentialTriangles(const BVHNode* node, const AABB& targetAABB, std::vector<int>& outIndices) const;
	
	// プレイヤーのAABBと重なる可能性のある三角形をすべて抽出する
	void QueryTriangles(const AABB& targetAABB, std::vector<PhysicsTriangle>& outTriangles) const;
private:
	// 再帰的にノードを構築(Buildで使用)
	std::unique_ptr<BVHNode> BuildRecursive(std::vector<PhysicsTriangle>& triangle, size_t start, size_t end);
	
	// 再帰的にノードを辿る
	void QueryRecursive(const BVHNode* node, const AABB& targetAABB, std::vector<PhysicsTriangle>& outTriangles) const;
private:
	// 構築したBVHの情報
	std::unique_ptr<BVHNode> root_ = nullptr;
	
	// BVHの構築に使用したTriangleの情報
	std::vector<PhysicsTriangle>allTriangles_;
};