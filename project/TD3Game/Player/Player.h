#pragma once
#include "ModelObject.h"
#include "PlayerStates.h"
#include "InputHandler.h"
#include "Umbrella.h"
#include "MotionManager.h"
#include "Collider.h"
#include "Ground.h"

import ManaComponent;
import StatusComponent;

using namespace PlayerStates;

enum class WeaponStance {
	Sheathed,// 納刀
	Drawn,   // 抜刀
};

struct PlayerInputData {
	Vector3 moveDirection;// 左スティックの入力方向
	float aimingDirectionX;// 右スティックの左右入力
	float aimingDirectionY;// 右スティックの上下入力
	bool isAttack;        // 攻撃ボタンが押された瞬間か
	bool isAttackHeld;
	bool isAttackReleased;
	bool isJump;		  // ジャンプボタンが押された瞬間か
	bool isEvasion;		  // 回避ボタンが押された瞬間か
	bool isSheathe;		  // 納刀ボタンが押されたか瞬間か
	bool useMana;         // マナを使用するかどうか
	bool isGuard;
	bool isReverse;
	bool isAiming;        // 照準を合わせているかどうか
	bool isAimingHeld;
	bool isShoot;         // 射撃したかどうか
	bool isRepair;        // 修理ボタンを押したかどうか
};

class Player {
public:
	Player(Fngine* fngine) : p_fngine(fngine) {
		InitializeStates();
	}
	~Player() {};
public:
	void Initialize();
	void Update(float deltaTime);
	void Draw();

	ConvenienceModel colliderTopper_[4];// 当たり判定の上に表示するモデル（デバッグ用）
	//////////////////////////
	/// 
	///   State関係
	/// 
	//////////////////////////
public:
	void ChangeMovementState(PlayerStates::Base* newState);
	void ChangeActionState(PlayerStates::Base* newState);
	bool onGround_ = false;
private:
	WeaponStance currentStance_ = WeaponStance::Drawn;
	

	// 使用しているステート
	PlayerStates::Base* currentMovementState_;
	PlayerStates::Base* currentActionState_;
	// 次に実行したいステート
	PlayerStates::Base* reservedActionState_ = nullptr;
public:
	// Movement State
	std::unique_ptr<Movement::Grounded>groundedState_;
	std::unique_ptr<Movement::Idle>idleState_;
	std::unique_ptr<Movement::Walking>walkingState_;
	std::unique_ptr<Movement::Running>runningState_;
	std::unique_ptr<Movement::Airborne>airborneState_;
	std::unique_ptr<Movement::Restricted>restrictedState_;// アクションの際に動きを制限するState

	// Action State
	std::unique_ptr<Action::SheatheWeapon>sheatheWeaponState_;
	std::unique_ptr<Action::DrawWeapon>drawWeaponState_;
	std::unique_ptr<Action::Normal>normalState_;
	std::unique_ptr<Action::Attack>attackState_;
	std::unique_ptr<Action::Guard>guardState_;
	std::unique_ptr<Action::ReverseCharge>reverseChargeState_;
	std::unique_ptr<Action::ReverseAttack>reverseAttackState_;
	std::unique_ptr<Action::ThrowUmbrella>throwUmbrellaState_;

	std::unique_ptr<Action::UmbrellaOpen>umbrellaOpenState_;
	std::unique_ptr<Action::UmbrellaClose>umbrellaCloseState_;
	std::unique_ptr<Action::UmbrellaReverse>umbrellaReverseState_;
	std::unique_ptr<Action::RepairUmbrella>repairUmbrellaState_;

public:// Get・Set
	WeaponStance GetWeaponStance() const { return currentStance_; }
	void SetWeaponStance(WeaponStance nextStance) { currentStance_ = nextStance; }
	// アクションを予約する
	void ReserveActionState(PlayerStates::Base* state) { reservedActionState_ = state; }
	// 予約を消費する
	PlayerStates::Base* ConsumeReservedAction() {
		PlayerStates::Base* state = reservedActionState_;
		reservedActionState_ = nullptr;
		return state;
	}
private:// 見る必要のない関数
	void InitializeStates();

	//////////////////////////////
	///
	///   移動関係の変数
	/// 
	//////////////////////////////
public:
	// 外部からの力を加える関数
	void AddForce(const Vector3& force);
	// ジャンプ
	void Jump();
	// ワープ
	void WarpToUmbrella();
public:
	// 移動制御用の変数
	Vector3 moveDirection_;// プレイヤーの移動したい方向
	Vector3 eyesDirection_;// プレイヤーの視線の方向
	// ※たぶん通常は同じ方向だが、ロックオンしているときは分かれる

	Vector3 myVelocity_;// 自発的な速度
	Vector3 externalVelocity_;// 外部からの速度(ノックバック、重力 etc)

	Vector3 moveAmount_;// 最終的な1フレームの移動量
	// ※加速度や速さはそれぞれのStateで各々作る

	// 【 ジャンプ 】
	float jumpCoyoteTimer_ = 0.0f;
	const float JUMP_COYOTE_MAX_TIME = 0.15f;

	//////////////////////////////
	///
	///   入力関係
	/// 
	//////////////////////////////
private:
	InputHandler inputHandler_;
	PlayerInputData inputData_;
	void ThrowUpdate(float deltaTime);
public:
	// Get・Set関係
	void SetInputData(const PlayerInputData& input) { inputData_ = input; }
	const PlayerInputData& GetInput()const { return inputData_; }

	// ====================
	// 照準・発射
	// ====================
public:
	void SetTargetPos(const Vector3& pos) { targetPos_ = pos; }
	Vector3 GetTargetPos()const { return targetPos_; }
private:
	Vector3 targetPos_;// ターゲットの位置
	ConvenienceModel targetMarker_;// ターゲットマーカー

	//////////////////////////////
	///
	///   アタッチメント
	/// 
	//////////////////////////////
public:
	Attachment* GetRightHandJoint() { return &rightHandJoint_; }
	Attachment* GetBackJoint() { return &backJoint_; }

	void UmbrellaAttachRHand();
	void UmbrellaAttachBack();
private:
	Attachment rightHandJoint_;
	Attachment backJoint_;

	//////////////////////////////
	///
	///   傘
	/// 
	//////////////////////////////
public:
	Umbrella::Main& GetUmbrella() { return *umbrella_; }
private:
	std::unique_ptr<Umbrella::Main>umbrella_;

	//////////////////////////////
	///
	///   Component
	/// 
	//////////////////////////////
public:
	// Get関係
	ManaComponent& GetManaComponent() { return *mana_; }
	StatusComponent& GetStatusComponent() { return *status_; }
private:
	// ManaComponent
	std::unique_ptr<ManaComponent>mana_;
	// StatusComponent
	std::unique_ptr<StatusComponent>status_;

	//////////////////////////////
	///
	///   当たり判定
	/// 
	//////////////////////////////
public:
	Collider* GetCollider()const { return collider_.get(); }

private:
	std::unique_ptr<ConvexCollider>collider_;

	//////////////////////////////
	///
	///   その他
	/// 
	//////////////////////////////
private:
	Fngine* p_fngine;

	std::unique_ptr<ModelObject> obj_;

	// プレイヤーの行動を管理するクラス
public:	std::unique_ptr<MotionController> motionController_;

	  /*OnGroundの実装必要だわ。浮いちゃう*/

public:
	Vector3 GetPosition()const { return obj_->worldTransform_.get_.Translation(); }
	void SetPosition(const Vector3& pos) { obj_->worldTransform_.set_.Translation(pos); }
private:
	void InitializeComponents();
};
