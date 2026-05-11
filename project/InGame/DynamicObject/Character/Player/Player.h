#pragma once
#include "Character.h"

class Player : public Character
{
public:
	Player();
	~Player()override = default;

///////////////////////////
/// 
/// 基本的な存在
///
//////////////////////////
public:
	void Initialize(Fngine* fngine);
	void Update(float deltaTime)override;
	void Draw()override;

private:
	std::unique_ptr<Skeleton>skeleton_ = nullptr;
///////////////////////////
/// 
/// 物理的な存在達
///
//////////////////////////
public:


private:
	// 【 ジャンプ 】
	float jumpCoyoteTimer_ = 0.0f;// 地面を離れた後もジャンプを受け付ける猶予期間
	const float JUMP_COYOTE_MAX_TIME = 0.15f;

};

