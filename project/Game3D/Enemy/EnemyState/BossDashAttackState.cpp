#include "EnemyState.h"
#include "Enemy.h"

void BossDashAttackState::Initialize() {
	if (!boss_)return;
	timer_ = 0.0f;
	phase_ = DashPhase::Prepare;

	// 予備動作開始時にプレイヤーの方を向く
	boss_->LookAtTarget();

	// 一旦色を変える
	boss_->SetColor({ 0.0f,0.0f,0.0f,1.0f });
}

void BossDashAttackState::Update() {
	if (!boss_)return;
	const float deltaTime = 1.0f / 60.0f;
	timer_ += deltaTime;
	Vector3 move = dashDirection_ * dashSpeed_ * deltaTime;
	switch (phase_) {
	case DashPhase::Prepare:
		// 溜め期間：移動停止
		boss_->SetMovement({ 0.0f, 0.0f, 0.0f });

		if (timer_ >= prepareDuration_) {
			phase_ = DashPhase::Dash;
			timer_ = 0.0f;
			// 突進する方向を確定させる
			dashDirection_ = boss_->GetForwardVector();
		}
		break;

	case DashPhase::Dash:
		// 突進期間：高速移動
	
		
		boss_->SetMovement(move);

		// 突進中も当たり判定を出す
		boss_->EnableAttackHitBox(true, boss_->GetPosition(), 2.5f);
	

		if (timer_ >= dashDuration_) {
			phase_ = DashPhase::Recover;
			timer_ = 0.0f;
			boss_->EnableAttackHitBox(false, { 0,0,0 }, 0);
		}
	break;

	case DashPhase::Recover:
		// 硬直期間：大きな隙
		boss_->SetMovement({ 0.0f, 0.0f, 0.0f });
		if (timer_ >= recoverDuration_) {
			boss_->ChangeState(new BossDecisionState());
		}
		break;
	}
}

void BossDashAttackState::Exit() {
	boss_->EnableAttackHitBox(false, { 0,0,0 }, 0);
	// 変えた色を戻す
	boss_->SetColor({ 1.0f,1.0f,1.0f,1.0f });
}