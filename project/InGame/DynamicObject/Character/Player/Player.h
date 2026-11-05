#pragma once
#include "Character.h"
#include "Animation.h"
#include "../../../../Engine/Objects/Hair/IHair.h"

struct HitEffectInfo {
	WorldTransform transform;
	float lifeTime;
	Vector4 color;
	float currentTime;
};

class Player : public DynamicObject
{
public:
	Player();
	~Player()override = default;

	void TakeDamage();

///////////////////////////
/// 
/// 基本的な存在
///
//////////////////////////
public:
	void Initialize(Fngine* fngine);
	void Update(float deltaTime)override;
	void Draw()override;

///////////////////////////
/// 
/// 一時的なParticleの存在
///
//////////////////////////
public:
	void MakeHitEffect();

private:
	std::vector<HitEffectInfo>hitEffect_;
	float hitEffectCoolTimer_ = 0.0f;

public:
	// ビームCollider
	//std::unique_ptr<MeshCollider>beamCollider_ = nullptr;

///////////////////////////
/// 
/// 髪のための関数
///
//////////////////////////
public:
	Matrix4x4 GetHeadMatrix()const;
	Vector3 GetHeadPos()const;
	void SetHeadPosToCameraTarget();
	Hair* GetHair(){ return hair_.get(); }

private:
	std::unique_ptr<Hair>hair_;

};
