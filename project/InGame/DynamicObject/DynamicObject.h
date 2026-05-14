#pragma once
#include "ModelObject.h"
#include "Collider.h"
#include "StatusComponent.h"

class DynamicObject
{
public:
	DynamicObject() = default;
	virtual ~DynamicObject() = default;

///////////////////////////
/// 
/// 基本的な存在
///
//////////////////////////
public:
	/// <summary>
	/// 基本的な初期化処理
	/// </summary>
	/// <param name="engine"></param>
	/// <param name="modelName">使うModelの名前 (例)mesh.objならmesh</param>
	/// <param name="textureName">使うTextureの名前(例)GridLine.pngならGridLine</param>
	virtual void Initialize(Fngine* engine, std::string modelName = "cube", std::string textureName = "GridLine");
	/// <summary>
	/// ステータス、等の更新処理
	/// </summary>
	/// <param name="deltaTime"></param>
	virtual void Update(float deltaTime);
	virtual void Draw();
	
protected:
	std::unique_ptr<ModelObject> obj_;

///////////////////////////
/// 
/// ステータス関係
///
//////////////////////////
public:
	StatusComponent& GetStatus() { return status_; }

protected:
	StatusComponent status_;

///////////////////////////
/// 
/// 物理的な存在達
///
//////////////////////////
public:
	bool OnGround()const { return onGround_; }
	void SetMyVelocity(const Vector3& velocity) { myVelocity_ = velocity; }
	Vector3 GetMyVelocity()const { return myVelocity_; }
	float GetCurrentGroundFriction()const { return currentGroundFriction_; }
protected:
	Vector3 moveAmount_;// 最終的な移動量

	// 速度関係
	Vector3 myVelocity_;// 自発的な速度
	Vector3 externalVelocity_;// 外部からの速度(ノックバック、重力 etc)

	float currentGroundFriction_ = 0.0f;// 現在の地面の摩擦力

	bool onGround_ = false;// 地面にいるかの判断

	float gravity_ = 9.8f;
	const float MAX_FALL_VELOCITY = -2.5f;
///////////////////////////
/// 
/// Collider関係
///
//////////////////////////
public:
	Collider* GetCollider() { return collider_.get(); }
protected:
	void OnCollisionGround(Collider* other, const Vector3& outPush);

protected:
	template<typename T, typename ...ARGs>
	T* CreateCollider(ARGs&&... args) {
		auto newCollider = std::make_unique<T>(std::forward<ARGs>(args)...);

		T* rawPtr = newCollider.get();

		collider_ = std::move(newCollider);

		collider_->SetUserData(this);

		return rawPtr;
	}

protected:
	std::unique_ptr<Collider>collider_;
	std::unique_ptr<BVH>bvh_;

};

