#pragma once
#include "Structures.h"
#include "Quaternion.h"

class Player3D;

class PlayerState
{
public:
	PlayerState() = default;
	virtual ~PlayerState() = default;
public:
	virtual void Initialize() = 0;
	virtual void Update() = 0;
	// 終了時に呼び出す処理
	virtual void Exit() {};
	void SetPlayer(Player3D* player) { player_ = player; }
protected:
	Player3D* player_ = nullptr;
};

class PlayerStopState
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
	void Exit()override;
private:

};

class PlayerWalkState
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
	void Exit()override;
private:

};

class PlayerJumpState
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
	void Exit()override;
private:
	// ジャンプの初期速度設定、空中移動、着地判定など
	float initial_verticalVelocity = 0.5f;
};

class PlayerFallState
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
	void Exit()override;
private:
	// 落ちているときの挙動とかetc...

};

class PlayerDoubleJumpState
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
	void Exit()override;
private:
	// 二段ジャンプ時の挙動、残りのジャンプ回数管理など
	float initial_verticalVelocity = 0.5f;
};

class PlayerDashState
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
	void Exit()override;
private:
	// 加速に関して
	float currentSpeedMultiplier_ = 0.0f;// 現在の速度倍率
	float initial_speedMultiplier_ = 1.0f;// ダッシュ開始時の速度
	float maxSpeedMultiplier_ = 5.0f;// ダッシュの最大速度
	float accelerationRate_ = 1.2f;// １秒当たりの速度上昇率
	// スタミナに関して
	float initial_staminaCost_ = 10.0f;// ダッシュ開始時の固定消費
	float staminaDrainRate_ = 15.0f;// １秒あたりの継続消費量
};

class PlayerLowStanceState
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
private:
	// しゃがみ姿勢の当たり判定変更、移動制限など
};

// スタミナ切れ時の State
class PlayerExhaustedState
	: public PlayerState // PlayerStateを継承
{
public:
	void Initialize() override;
	void Update() override;
	void Exit() override;
private:
	float STAMINA_REGEN_DELAY = 1.0f; // スタミナ回復が始まるまでの遅延時間
	float exhaustedTimer_ = 0.0f; // 遅延時間を計測するタイマー
	float EXHAUSTED_REGEN_THRESHOLD = 70.0f; // 復帰に必要なスタミナの閾値（例: 30%）
};

enum class AttackTrajectory {
	VerticalSrash,   // 縦切り
	HorizontalSwing, // 横切り
	SpinAttack,      // 回転切り
	Thrust,          // 突き
};
// 名前、発生時間、持続時間、全体硬直、コンボ受付開始、コンボ受付終了、ダメージ量、次の攻撃のIndex、使う軌跡、攻撃範囲
struct AttackData {
	const char* name;   // 名前
	float startup;      // 発生までの時間
	float active;       // 持続時間
	float recovery;     // 全体硬直
	float comboStart;   // コンボ受付開始
	float comboEnd;     // コンボ受付終了
	float damage;       // ダメージ量
	int nextComboIndex; // 次の攻撃のIndex
	AttackTrajectory trajectory;// 使う軌跡
	float range;        // 攻撃範囲
};

class PlayerAttackState
	:public PlayerState
{
public:
	PlayerAttackState(int comboIndex) :currentComboIndex_(comboIndex) {}
public:
	void Initialize() override;
	void Update() override;
	void Exit() override;
private:
	Vector3 CalculateAttackOffset(const AttackData& data, float slashTime);
private:
	float attackTimer_ = 0.0f;          // 攻撃アニメーションの経過時間
	float maxAttackDuration_ = 0.5f; // 攻撃アニメーションの持続時間
	// コンボ受付期間を管理する変数
	int currentComboIndex_;
};

class PlayerGuardState
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
private:
	// ガード判定の生成、被ダメージ計算、移動制限
};

// ------------------------
// EVASION STATE
// ------------------------

class PlayerEvasionState
	: public PlayerState
{
public:
	void Initialize() override;
	void Update() override;
	void Exit() override;

private:
	float evasionTimer_ = 0.0f;               // 回避の経過時間
	float EVASION_DURATION = 0.3f;      // 回避の持続時間（0.5秒）
	float EVASION_SPEED_RATE = 3.0f;        // 一回で進む距離
	float initial_verticalVelocity = 0.75f;// 縦に飛ぶ距離
	float targetAngle_ = 360.0f;          // 回転させたい量
	float initial_staminaCost_ = 10.0f;// 回避開始時の固定消費

	Vector3 evasionDirection_{ 0.0f, 0.0f, 0.0f }; // 回避する方向のベクトル
	Quaternion preQuaternion_{};        // 回避する瞬間の回転を保持（将来バグの原因の可能性あり）
};

class PlayerStepEvasionState
	: public PlayerState
{
public:
	~PlayerStepEvasionState()override = default;
public:
	void Initialize()override;
	void Update()override;
	void Exit()override;

private:
	float evasionTimer_ = 0.0f;// 回避の経過時間
	const float evasionDuration_ = 0.2f;// 回避の持続時間(ローリングより短い)
	const float evasionDistance_ = 1.0f;// 回避で進む距離
	const float initial_staminaCost_ = 5.0f;// 回避時の固定消費コスト

	Vector3 evasionDirection_{ 0.0f, 0.0f,0.0f };// 回避する方向のベクトル
};

class PlayerJustEvasionState
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
	void Exit()override;
private:
	// ジャスト回避成功時の特殊効果（スローモーション、反撃機会など）
	float evasionTimer_ = 0.0f;
	const float evasionDuration_ = 0.2f;// 回避の持続時間(ローリングより短い)
	const float evasionDistance_ = 1.0f;// 回避で進む距離
	Vector3 evasionDirection_{ 0.0f, 0.0f,0.0f };// 回避する方向のベクトル
};

class PlayerJustGuardState
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
private:
	// ジャストガード成功時の特殊演出や反撃準備
};

class PlayerAerialComboState
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
private:
	// 空中コンボ中の挙動、落下速度の制御、特殊な空中攻撃
};

class PlayerCounterState
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
private:
	// カウンターの入力受付期間と、成功時の攻撃処理
};

class PlayerSheathState
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
private:
	// 納刀アニメーションの管理、完了後の非戦闘状態への遷移
};

class PlayerDrawState
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
private:
	// 抜刀アニメーションの管理、完了後の戦闘状態への遷移
};

// ** 特殊なアクション・状態 **

class PlayerUseItemState
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
private:
	// アイテム使用アニメーション、効果の発動、硬直時間の管理
};

class PlayerSpecialAttackState // 特別なゲージを使用した攻撃
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
private:
	// ゲージ消費、派手な攻撃演出と判定
};

// 魔法の基本状態。種類によってさらに細分化
class PlayerCastMagicState
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
private:
	// 詠唱時間、魔法の選択、発射ロジック
};

class PlayerChargeAttackState // 溜め攻撃
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
private:
	// 溜め時間の計測、エフェクトの表示、最大溜め判定
};

class PlayerReloadState
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
private:
	// リロードアニメーション、弾数の回復、リロード時間の管理
};

class PlayerInteractState
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
private:
	// 調べたり、インタラクトする際の専用アニメーションやメッセージ表示
};

class PlayerCancelActionState
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
private:
	// アクションを中断し、次の状態へスムーズに遷移させるためのロジック
};

class PlayerFeintState
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
private:
	// フェイント専用のアニメーション、硬直の有無など
};

// ** 状態異常・その他 **

class PlayerStatusEffectState // 状態異常
	:public PlayerState
{
public:
	void Initialize()override;
	void Update()override;
private:
	// 毒や麻痺などの効果処理、タイマー管理
};

class PlayerDeathState
	: public PlayerState
{
public:
	void Initialize() override;
	void Update() override;
	void Exit() override {} // 死亡 State から Exit する際は特別な処理は不要（シーン遷移を想定）

private:
	float deathTimer_ = 0.0f;           // 死亡演出の経過時間
	float MAX_DEATH_DURATION = 3.0f; // 死亡演出の持続時間（3秒間を想定）
};

class PlayerHurtState
	: public PlayerState
{
public:
	void Initialize() override;
	void Update() override;
	void Exit() override;

private:
	float hurtTimer_ = 0.0f;          // のけぞりの経過時間
	float MAX_HURT_DURATION = 0.8f;
};