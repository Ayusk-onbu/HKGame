#pragma once
#include "Structures.h"
#include "AABB.h"
#include "BVH.h"

#include <functional>

enum COLLISIONATTRIBUTE :int{
	COL_None = 0,               // 特になし
	COL_Player = 1 << 0,        // 
	COL_Enemy = 1 << 1,         // 
	COL_Player_Attack = 1 << 2, // 
	COL_Enemy_Attack = 1 << 3,  //
	COL_Ground = 1 << 4,        // 
	COL_Static_Map = 1 << 5,    // 動かないマップ
	COL_ICE = 1 << 6,           // 氷（滑る床など）
};

enum class ColliderShape {
	Convex,
	Sphere,
	Capsule,
	Triangle
};

class Collider
{
public:
	Collider() = default;
	virtual ~Collider() = default;

public:
	using CollisionCallBack = std::function<void(Collider*, const Vector3& outPush)>;
	CollisionCallBack onCollisionCallBack = nullptr;

	virtual void OnCollision(Collider* other, const Vector3& outPush) {
		if (enableHitHistory) {
			if (std::find(hitHistory_.begin(), hitHistory_.end(), other) != hitHistory_.end()) {
				return; // 既に当たっているので無視！
			}
			// 新しい相手なら履歴に追加
			hitHistory_.push_back(other);
		}

		// 履歴チェックを抜けた場合のみ、コールバック（ダメージ処理など）を呼ぶ
		if (onCollisionCallBack) {
			onCollisionCallBack(other, outPush);
		}
	}
	// --- 各形状に合わせてAABBを再計算する純粋仮想関数 ---
	virtual void UpdateAABB() = 0;

	// なにかしたい処理があればここに書く
	virtual void Update(){}

	// --- 形状判定用 ---
	virtual ColliderShape GetShapeType() const = 0;

	virtual Vector3 FindFurthestPoint(Vector3 direction) const = 0;
public:
	/////////////////////////////////
	/// 
	///   Get・Set
	///
	/////////////////////////////////

	Vector3 const& GetWorldPosition() const noexcept { return worldPosition_; }
	void SetWorldPosition(const Vector3& pos) { worldPosition_ = pos; }

	AABB const& GetAABB() const noexcept { return aabb_; }
	void SetAABB(const AABB& aabb) { aabb_ = aabb; }

	uint32_t GetMyType() const noexcept { return collisionAttribute_; }
	void SetMyType(uint32_t type) { collisionAttribute_ = type; }

	uint32_t GetYourType() const noexcept { return collisionMask_; }
	void SetYourType(uint32_t type) { collisionMask_ = type; }
protected:
	// AABB
	AABB aabb_;

	Vector3 worldPosition_ = { 0.0f,0.0f,0.0f };

	uint32_t collisionAttribute_ = 0xffffffff;
	uint32_t collisionMask_ = 0xffffffff;
public:
	// 持ち主（PlayerやEnemyなど）を登録・取得するための関数
	void SetUserData(void* userData) { userData_ = userData; }
	void* GetUserData() const { return userData_; }

private:
	void* userData_ = nullptr; // 持ち主のポインタを保存

/////////////////////////////////
/// 
///   当たり判定の保存に関する機能
///
/////////////////////////////////
public:
	// 履歴をリセットする関数（時間経過やアニメーションで呼ぶ）
	void ClearHitHistory() { hitHistory_.clear(); }
	void SetEnableHitHistory(bool enable) { enableHitHistory = enable; }
private:
	bool enableHitHistory = false; // デフォルトはOFF（体や壁用）
	std::vector<Collider*> hitHistory_;
};

class MeshCollider : public Collider
{
public:
	MeshCollider() {
		worldMatrix_ = std::make_unique<Matrix4x4>();
		*worldMatrix_ = Matrix4x4::Make::Identity();
	}
	virtual ~MeshCollider() {}

public:
	ColliderShape GetShapeType() const override { return ColliderShape::Convex; }

	Vector3 FindFurthestPoint(Vector3 direction) const override {

		Vector3  maxPoint;
		float maxDistance = -FLT_MAX;

		for (Vector3 vertex : worldVertices_) {

			float distance = Dot(vertex, direction);
			if (distance > maxDistance) {
				maxDistance = distance;
				maxPoint = vertex;
			}
		}

		return maxPoint;
	}

	// GJKに必要な頂点データ
	void SetVertices(std::vector<Vector3> const& vertices) { vertices_ = vertices; }
	std::vector<Vector3> const& GetVertices() const { return vertices_; }
	void ClearVertices() { vertices_.clear(); }

	// 自身の頂点群からAABBを計算して更新する
	void UpdateAABB() override;

	void Update()override;
public:
	// PositionではなくMatrixを持たせる
	void SetWorldMatrix(Matrix4x4 const& mat) { *worldMatrix_ = mat; }
	Matrix4x4 const& GetWorldMatrix() const { return *worldMatrix_; }

private:
	std::vector<Vector3> vertices_;
	std::vector<Vector3> worldVertices_;
	std::unique_ptr<Matrix4x4> worldMatrix_;
};


// 静的なMapのためのもの
class TriangleCollider : public Collider {
public:
	TriangleCollider(const PhysicsTriangle& tri) : triangle_(tri) {
		// AABBを初期化（PhysicsTriangle側の関数を使用）
		aabb_ = triangle_.GetAABB();
		collisionAttribute_ = COL_Static_Map;
	}
	virtual ~TriangleCollider() = default;

	ColliderShape GetShapeType() const override { return ColliderShape::Triangle; }

	// GJKアルゴリズムが呼ぶ「一番遠い点」を探す関数
	Vector3 FindFurthestPoint(Vector3 direction) const override {
		Vector3 maxPoint = triangle_.v0;
		float maxDistance = Dot(triangle_.v0, direction);

		float dist1 = Dot(triangle_.v1, direction);
		if (dist1 > maxDistance) { maxDistance = dist1; maxPoint = triangle_.v1; }

		float dist2 = Dot(triangle_.v2, direction);
		if (dist2 > maxDistance) { maxDistance = dist2; maxPoint = triangle_.v2; }

		return maxPoint;
	}

	// マップは動かないので空のまま
	void UpdateAABB() override {}

	void Update()override {}

private:
	PhysicsTriangle triangle_;
};

