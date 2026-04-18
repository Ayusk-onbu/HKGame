#pragma once
#include "Player3D.h" // ターゲットとして参照するため
#include "EnemyState.h" 
#include "Collider.h" // 攻撃判定用Collider (プレイヤー同様、位置設定可能)
#include "ModelObject.h"
#include "BossBodyCollider.h"
#include "BulletManager.h"

class BossEnemy
{
public:
	void Initialize(Fngine* fngine,Player3D* target);
	void Update();
	void Draw();
	~BossEnemy() {};

public:
	bool StartCutscene(float time);

	// State 遷移メソッド (Player3Dと同様)
	void ChangeState(BossState* newState);

	void LookAtTarget();

	Vector3 GetForwardVector();

	// ターゲットの位置情報取得
	Vector3 GetTargetPosition() const { return target_->GetPosition(); }

	Vector3 GetPosition() { return obj_->worldTransform_.GetWorldPos(); }

	void SetMovement(const Vector3& moveAmount) { moveAmount_ = moveAmount; }

	void SetColor(const Vector4& color) { obj_->SetColor(color); }

	// HP / ダメージ処理
	void TakeDamage(float damage);
	bool IsDead() const { return hp_ <= 0.0f; }

	float GetHP()const { return hp_; }
	float GetMaxHP()const { return maxHp_; }

	// Collider 制御
	// player_->EnableHitBox と同様の機能を BossEnemy にも実装
	void EnableAttackHitBox(bool enable, const Vector3& worldPos, float radius);

	//BossBodyCollider& GetBossBodyCollider() { return *bodyCollider_.get(); }
	//AttackCollider& GetAttackCollider() { return *attackCollider_.get(); }

	BulletManager& GetBulletManager() { return *bullets_.get(); }

	void SetIsViewAttack(bool flag) { isViewAttack_ = flag; }
private:
	void HPBarUpdate();

private:
	// --- 状態管理 ---
	BossState* state_ = nullptr;
	Player3D* target_ = nullptr; // プレイヤーへのポインタ

	std::unique_ptr<ModelObject>obj_;
	Vector3 moveAmount_ = { 0.0f,0.0f,0.0f };

	// --- パラメータ ---
	float hp_ = 500.0f;
	float maxHp_ = 500.0f;

	std::unique_ptr<SpriteObject>mainHPBar_;
	std::unique_ptr<SpriteObject>subHPBar_;

	// --- 衝突判定 ---
	// ボス本体の Collider (Body)
	//std::unique_ptr<BossBodyCollider> bodyCollider_;
	std::unique_ptr<ModelObject>bodyColliderObj_;
	float bossBodyRadius = 4.0f;
	// 攻撃用の Collider (Attack)
	//std::unique_ptr<AttackCollider> attackCollider_;
	bool isViewAttack_ = false;
	// ロックオン
	bool lockOn_ = false;

	// 弾
	std::unique_ptr<BulletManager>bullets_;

	// 炎
	std::vector<std::unique_ptr<Particle>>fire_;

	// --- AI/行動管理 ---
	// クールダウン管理用のマップなど
	int phase_ = 1;
};

