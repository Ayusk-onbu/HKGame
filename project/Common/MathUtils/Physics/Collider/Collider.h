#pragma once
#include "Structures.h"
#include "AABB.h"
#include <vector>
#include <functional>

enum COLLISIONATTRIBUTE :int{
	COL_None = 0,
	COL_Player = 1 << 0,
	COL_Enemy = 1 << 1,
	COL_Player_Attack = 1 << 2,
	COL_Enemy_Attack = 1 << 3,
	COL_Ground = 1 << 4,
};

enum class ColliderShape {
	Sphere,
	AABB,
	Convex // GJK用
};

class Collider
{
public:
	virtual ~Collider() = default;
public:
	using CollisionCallback = std::function<void(Collider*, const Vector3&)>;
	CollisionCallback onCollisionCallback = nullptr;

	virtual void OnCollision(Collider* other, const Vector3& pushOut) {
		if (onCollisionCallback) {
			onCollisionCallback(other, pushOut);
		}
	}

	// --- 各形状に合わせてAABBを再計算する純粋仮想関数 ---
	virtual void UpdateAABB() = 0;

	// --- 形状判定用 ---
	virtual ColliderShape GetShapeType() const = 0;
public:
	/////////////////////////////////
	/// 
	///   Get・Set
	///
	/////////////////////////////////

	const Vector3 GetWorldPosition(){ return worldPosition_; }
	void SetWorldPosition(const Vector3& pos) { worldPosition_ = pos; }

	AABB GetAABB()const { return aabb_; }
	void SetAABB(const AABB& aabb) { aabb_ = aabb; }

	const uint32_t& GetMyType()const { return collisionAttribute_; }
	void SetMyType(const uint32_t& type) { collisionAttribute_ = type; }
	
	const uint32_t& GetYourType()const { return collisionMask_; }
	void SetYourType(const uint32_t& type) { collisionMask_ = type; }
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
};

class ConvexCollider : public Collider
{
public:
	ColliderShape GetShapeType() const override { return ColliderShape::Convex; }

	// GJKに必要な頂点データ
	void SetVertices(const std::vector<Vector3>& vertices) { vertices_ = vertices; }
	const std::vector<Vector3>& GetVertices() const { return vertices_; }

	// 自身の頂点群からAABBを計算して更新する
	void UpdateAABB() override;
public:
	// PositionではなくMatrixを持たせる
	void SetWorldMatrix(const Matrix4x4& mat) { worldMatrix_ = mat; }
	const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }

private:
	std::vector<Vector3> vertices_;
	Matrix4x4 worldMatrix_;
};

