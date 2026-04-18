#include "BVH.h"
#include <algorithm>

// 箱に入る最大の数
constexpr int kMaxTrianglesPerLeaf = 4;

void BVH::Build(std::vector<PhysicsTriangle>& triangles) {
	if (triangles.empty())return;

	// 全三角形をBVH内部にコピーして保持
	allTriangles_ = triangles;

	// 0番目から最後まで構築
	root_ = BuildRecursive(triangles, 0, triangles.size());
}

std::unique_ptr<BVHNode> BVH::BuildRecursive(std::vector<PhysicsTriangle>& triangles, size_t start, size_t end) {
	auto node = std::make_unique<BVHNode>();
	// ノードを初期化
	node->bounds.ResetToExtreme();

	// 範囲上に存在する三角形を全て包むAABBを作成
	for (size_t i = start; i < end; ++i) {
		node->bounds.Expand(triangles[i].GetAABB());
	}

	// 含まれている三角形の数
	size_t count = end - start;

	// 再帰終了条件：三角形が最大数より少ない
	if (count < kMaxTrianglesPerLeaf) {
		for (size_t i = start; i < end;++i) {
			node->triangleIndices.push_back(triangles[i].originalIndex);
		}
		return node;
	}

	// AABBを分割する軸（ X, Y, Z ）を決める
	Vector3 size = node->bounds.GetSize();
	int splitAxis = 0;// 0 : X, 1 : Y, 2 : Z
	if (size.y > size.x && size.y > size.z) splitAxis = 1;
	if (size.z > size.x && size.z > size.y) splitAxis = 2;

	// 決定した軸で位置順にソートする
	std::sort(triangles.begin() + start, triangles.begin() + end,
		[splitAxis](const PhysicsTriangle& a, const PhysicsTriangle& b) {
			Vector3 centerA = a.GetCenter();
			Vector3 centerB = b.GetCenter();

			if (splitAxis == 0) return centerA.x < centerB.x;
			if (splitAxis == 1) return centerA.y < centerB.y;
			return centerA.z < centerB.z; // splitAxis == 2
		});

	// 真ん中で真っ二つにして、左と右の子ノードを再帰的に作る
	size_t mid = start + count / 2;
	node->left = BuildRecursive(triangles, start, mid);
	node->right = BuildRecursive(triangles, mid, end);

	return node;
}

void BVH::CollectPotentialTriangles(const BVHNode* node, const AABB& targetAABB, std::vector<int>& outIndices) const {
	if (!node) return;

	// そもそもこの箱(ノード)とプレイヤーが当たっていなければ、この箱の中身は全部無視
	if (!AABB::IsHitAABB2AABB(targetAABB, node->bounds)) {
		return;
	}

	// 葉っぱまで辿り着いたら、中に入っている三角形のインデックスをリストに追加！
	if (node->IsLeaf()) {
		for (int idx : node->triangleIndices) {
			outIndices.push_back(idx);
		}
		return;
	}

	// 3. 葉っぱじゃないなら、左右の子供の箱をさらに調べる
	CollectPotentialTriangles(node->left.get(), targetAABB, outIndices);
	CollectPotentialTriangles(node->right.get(), targetAABB, outIndices);
}

// 外部から呼ぶための入り口
void BVH::QueryTriangles(const AABB& targetAABB, std::vector<PhysicsTriangle>& outTriangles) const {
	outTriangles.clear();
	if (!root_) return;
	QueryRecursive(root_.get(), targetAABB, outTriangles);
}

// 再帰的に箱をチェックして三角形を拾い集める
void BVH::QueryRecursive(const BVHNode* node, const AABB& targetAABB, std::vector<PhysicsTriangle>& outTriangles) const {
	// 1. 今のノードのAABBとプレイヤーが当たっていなければ、その先（子供）は見なくて良い
	if (!AABB::IsHitAABB2AABB(node->bounds, targetAABB)) {
		return;
	}

	// 2. 葉っぱノードなら、中に入っているインデックスを使って三角形をコピー
	if (node->IsLeaf()) {
		for (int indexInSortedArray : node->triangleIndices) {
			// allTriangles_ から実データを取得
			// ※BuildRecursiveでソートされるため、その時点のインデックスを保持しておく必要があります
			outTriangles.push_back(allTriangles_[indexInSortedArray]);
		}
		return;
	}

	// 3. 葉っぱでないなら左右の子供をチェック
	if (node->left) QueryRecursive(node->left.get(), targetAABB, outTriangles);
	if (node->right) QueryRecursive(node->right.get(), targetAABB, outTriangles);
}