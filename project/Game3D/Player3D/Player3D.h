#pragma once
#include "ModelObject.h"
#include "PlayerState.h"
#include "Collider.h"
#include "PlayerBodyCollider.h"
#include "SpriteObject.h"
#include "Animation.h"
#include "PlayerInputHandler.h"
#include "Particle.h"

import MotionManager;

class Player3D
{
public:
	~Player3D();
public:
	std::unique_ptr<MeshCollider>collider_ = nullptr;

	void Initialize(Fngine* fngine);
	void Update();
	void Draw();
	void ImGui();
public:
	void ChangeState(PlayerState* newState);
	// 攻撃の当たり判定の変更
	void EnableHitBox(bool enale,const Vector3&worldPos);
	// 進んでいる方向に向く処理
	void RotateToMoveDirection();
public:
	// 垂直速度の設定と取得
	void SetVerticalVelocity(float velocity) { verticalVelocity_ = velocity; }
	float GetVerticalVelocity() const { return verticalVelocity_; }
	// 地面判定の取得
	void SetIsOnGround(bool flag) { isOnGround_ = flag; }
	bool IsOnGround()const { return isOnGround_; }
	// 二段ジャンプの設定と取得
	void SetCanDoubleJump(bool flag) { canDoubleJump_ = flag; }
	bool CanDoubleJump()const { return canDoubleJump_; }
	// スタミナの設定と取得
	void SetStamina(float cost) { stamina_ = cost; }
	float GetStamina() { return stamina_; }
	float GetStaminaRate() { return stamina_ / maxStamina_; }
	void DrainStamina(float amount) { stamina_ -= amount; }
	void BlockStaminaRecovery() { staminaRecoveryBlockers_++; }
	void UnblockStaminaRecovery() { if (staminaRecoveryBlockers_ > 0)staminaRecoveryBlockers_--; }
	// 移動速度倍率の設定と取得
	void SetSpeedMultiplier(float multiplier) { speedMultiplier_ = multiplier; }
	float GetSpeedMultiplier() { return speedMultiplier_; }
	// 状態チェック
	bool IsDead() const { return hp_ <= 0.0f; }
	bool IsInvulnerable() const { return isInvulnerable_; } // 無敵時間のチェック
	void SetInvulnerable(bool isInvulnerable) { isInvulnerable_ = isInvulnerable; }
	// [ 無敵回避フラグ ]
	bool IsJustEvasion()const { return isJustEvasion_; }
	void SetJustEvasion(bool isJustEvasion) { isJustEvasion_ = isJustEvasion; }

	//AttackCollider& GetAttackCollider() { return *attackCollider_.get(); }
	//PlayerBodyCollider& GetPlayerBodyCollider() { return *playerCollider_.get(); }

	// 位置の取得
	Vector3 GetPosition() { return obj_->worldTransform_.GetWorldPos(); }
	void TakeDamage(float damage);
	Vector3 GetForwardVector();
	// AttackColliderのRadiusの設定
	//void SetAttackColliderRadius(float radius) { attackCollider_->SetRadius(radius); }
	void SetAlpha(float alpha) { obj_->SetColor({ 1.0f,1.0f,1.0f,alpha }); }
	// Quaternionの設定・取得
	void SetPlayerQuaternion(const Quaternion& q) { obj_->worldTransform_.set_.Quaternion(q); }
	Quaternion GetPlayerQuaternion() { return obj_->worldTransform_.get_.Quaternion(); }
	// Move関係
	Vector3& Move() { return move_; }
	// Command
	// [ 移動 ]
	bool IsMoving()const { return (move_.x != 0.0f || move_.z != 0.0f); }
	void BlockMove() { moveBlockers_++; }
	void UnBlockMove() { if (moveBlockers_ > 0)moveBlockers_--; }
	// [ ジャンプ ]
	void BlockJump() { jumpBlockers_++; }
	void UnBlockJump() { if (jumpBlockers_ > 0)jumpBlockers_--; }
	bool CanJump()const { return jumpBlockers_ == 0; }
	// [ 攻撃 ]
	void BlockAttack() { attackBlockers_++; }
	void UnBlockAttack() { if (attackBlockers_ > 0)attackBlockers_--; }
	bool CanAttack()const { return attackBlockers_ == 0; }
	// [ ダッシュ ] 
	void DashBufferUpdate();
	bool CanDash()const { return (dashBufferTimer_ > 0.0f); }
	void ClearDashBuffer() { dashBufferTimer_ = 0.0f; }
	// [ 回避 ]
	void EvasionUpdate();
	void ExecuteEvasion(bool isLongPress);

	// Command Input
	void OnJumpInput();
	void OnAttackInput();
	void OnDashInput();
	void OnEvasionInput();
	// 先行入力関連
	// [ 攻撃 ]
	void ClearAttackBuffer();
	bool HasAttackBuffer()const { return isAttackBuffered_; }

	void SetIsViewAttack(bool flag) { isAttackViewFlag_ = flag; }
private:
	// 物理的な処理
	void ApplyPhysics();
	// スタミナ回復処理
	void UpdateStaminaRecovery();

	void ApplyGlobalVariables();
	// HPバー
	void HPBarUpdate();
private:
	std::unique_ptr<ModelObject>obj_;
	PlayerState* state_ = nullptr;
	// キャラのCollider
	//std::unique_ptr<PlayerBodyCollider>playerCollider_;

	// 攻撃判定用のCollider
	//std::unique_ptr<AttackCollider>attackCollider_;
	std::unique_ptr<ModelObject>attackColliderObj_;
	bool isAttackViewFlag_ = false;

	// 無敵管理フラグ
	bool isInvulnerable_ = false;
	bool isJustEvasion_ = false;

	// HP
	float maxHP_ = 100.0f;
	float hp_ = 100.0f;

	std::unique_ptr<SpriteObject>mainHPBar_;
	std::unique_ptr<SpriteObject>subHPBar_;

	// 入力
	std::unique_ptr<PlayerInputHandler> inputHandler_;
	// ブロッカー
	// [ 攻撃 ]
	int attackBlockers_ = 0;
	// 先行入力フラグ
	// [ 攻撃 ]
	bool isAttackBuffered_ = false;
	// [ ダッシュ ]
	float dashBufferTimer_ = 0.0f;
	float maxDashBufferTime_ = 0.2f;
	// [ 回避 ]
	float evasionPressTimer_ = 0.0f;
	bool isWaitingEvasion_ = false;

	//移動関係のしょり
	// 移動のディレクション
	Vector3 move_ = { 0,0,0 };
	// 一旦これは元のスピード
	float speed_ = 2.5f;
	// 移動速度倍率
	float speedMultiplier_ = 4.0f;
	// 動くことができるかどうか
	int moveBlockers_ = 0;

	// 旋回するスピード
	float rotateSpeed_ = 0.4f;

	float test_ = 0.0f;
	
	// スタミナ
	float stamina_ = 100.0f;
	std::unique_ptr<SpriteObject>staminaBar_;
	// マックススタミナ
	float maxStamina_ = 100.0f;
	// 1秒あたりの回復量
	float staminaRecoveryRate_ = 20.0f;
	// 回復を中断するかを確認するカウンター
	int staminaRecoveryBlockers_ = 0;

	// 垂直速度（Ｙ軸方向）
	float verticalVelocity_ = 0.0f;
	// 重力加速度
	float gravity_ = 1.0f;
	// 地面にいるかどうかのフラグ
	bool isOnGround_ = false;
	// ジャンプが可能かどうかのフラグ
	int jumpBlockers_ = 0;
	// 二段ジャンプが可能かのフラグ
	bool canDoubleJump_ = true;
	// ロックオン
	bool lockOn_ = false;

	// アニメーション
	std::unique_ptr<Animation>animation_ = nullptr;
	std::unique_ptr<Skeleton>skeleton_ = nullptr;

	MotionController motionController_;
};

