#pragma once
#include "Fngine.h"
#include "Collider.h"
#include "ModelObject.h"

//class BulletCollider final
//	: public AttackCollider
//{
//private:
//	class Bullet* bullet_ = nullptr;
//public:
//	// Bulletへのポインタを持ち、衝突時にBulletに通知する
//	BulletCollider(Bullet* bullet) : bullet_(bullet) {}
//	void OnCollision(Collider* other) override;
//
//};

class Bullet {
public:
	void Initialize(Fngine* fngine,const Vector3& pos, const Vector3& speed,int MyType,int yourType,float radius,float lifeTime);
	void Update();
	void Draw();

	bool IsDead()const { return isDead_; }
	void Dead();

	//BulletCollider& GetCollider() { return *collider_.get(); }
private:
	Fngine* fngine_;
	//std::unique_ptr<BulletCollider>collider_;
	std::unique_ptr<ModelObject>obj_;
	Vector3 velocity_;
	// 命
	float lifeTime_;
	bool isDead_ = false;
};