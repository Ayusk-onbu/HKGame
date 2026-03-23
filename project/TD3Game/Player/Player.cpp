#include "CameraSystem.h"
#include "ImGuiManager.h"
#include "Player.h"
#include <string>

using namespace PlayerStates;
void Player::Initialize() {
	obj_ = std::make_unique<ModelObject>();
	obj_->modelName_ = "player";
	obj_->textureName_ = "ulthimaSky";
	obj_->Initialize(p_fngine);

	InitializeStates();
	InitializeComponents();

	inputHandler_.SetPlayer(this);

	rightHandJoint_.SetType(AttachmentType::PlayerHand);
	rightHandJoint_.SetAcceptType(AttachmentType::UmbrellaHandle);
	rightHandJoint_.SetInfo({ 0.0f,-0.0f,0.0f }, { 0.0f,0.0f,0.0f });

	backJoint_.SetType(AttachmentType::PlayerBack);
	backJoint_.SetAcceptType(AttachmentType::UmbrellaHandle);
	backJoint_.SetInfo({ 0.0f,-0.0f,0.0f }, { 0.0f,0.0f,0.0f });
	backJoint_.SetRot({0.0f,0.0f,Deg2Rad(135)});

	umbrella_ = std::make_unique<Umbrella::Main>();
	umbrella_->Initialize(p_fngine);

	umbrella_->handle_->GetBaseJoint()->AttachTo(GetRightHandJoint());

	motionController_ = std::make_unique<MotionController>();
}

void Player::Update(float deltaTime) {
	// 移動量の初期化
	moveAmount_ = { 0.0f,0.0f,0.0f };

	// 入力関係の処理
	inputHandler_.HandleInput();

	// コンポーネントの更新
	if (mana_) {
		mana_->Update(deltaTime);
	}
	// ステートの更新
	if (currentMovementState_) {
		currentMovementState_->Update(deltaTime);
	}
	if (currentActionState_) {
		currentActionState_->Update(deltaTime);
	}

	// ここから移動関係の処理
	moveAmount_ = (myVelocity_ + externalVelocity_) * deltaTime;

	obj_->worldTransform_.set_.Translation(obj_->worldTransform_.get_.Translation() + moveAmount_);

	// rightHandJoint_.SetRot( 手の回転 );
	rightHandJoint_.Update(); // 右手Joint自身の行列を計算

	Vector3 backPos = obj_->worldTransform_.get_.Translation();
	backPos.y += 1.5f;
	backPos.z += 1.0f;

	backJoint_.SetPos(backPos);
	backJoint_.Update();
	
	// 傘
	umbrella_->Update(deltaTime);

	Vector3 test = rightHandJoint_.GetPos();
	ImGui::DragFloat3("RHandJoint", &test.x);
}

void Player::Draw() {
	obj_->LocalToWorld();
	obj_->SetWVPData(CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(obj_->worldTransform_.mat_));
	obj_->Draw();

	umbrella_->Draw();
}

void Player::ChangeMovementState(PlayerStates::Base* newState) {
	if (currentMovementState_) {
		currentMovementState_->Exit();
	}

	currentMovementState_ = newState;

	if (currentMovementState_) {
		currentMovementState_->Enter();
	}
}

void Player::ChangeActionState(PlayerStates::Base* newState) {
	if (currentActionState_) {
		currentActionState_->Exit();
	}

	currentActionState_ = newState;

	if (currentActionState_) {
		currentActionState_->Enter();
	}
}

void Player::InitializeStates() {

	//// MovementStateの初期化 ////

	// 親のStateの生成
	groundedState_ = std::make_unique<Movement::Grounded>();groundedState_->SetInfo(this);
	airborneState_ = std::make_unique<Movement::Airborne>();airborneState_->SetInfo(this);

	// 子のStateの生成
	idleState_ = std::make_unique<Movement::Idle>();idleState_->SetInfo(this, groundedState_.get());
	walkingState_ = std::make_unique<Movement::Walking>();walkingState_->SetInfo(this, groundedState_.get());
	runningState_ = std::make_unique<Movement::Running>();runningState_->SetInfo(this, groundedState_.get());

	// 最初の設定
	currentMovementState_ = idleState_.get();

	//// ActionStateの初期化 ////

	sheatheWeaponState_ = std::make_unique<Action::SheatheWeapon>();sheatheWeaponState_->SetInfo(this);
	drawWeaponState_ = std::make_unique<Action::DrawWeapon>();drawWeaponState_->SetInfo(this);
	normalState_ = std::make_unique<Action::Normal>();normalState_->SetInfo(this);
	attackState_ = std::make_unique<Action::Attack>();attackState_->SetInfo(this);

	// 最初の設定
	currentActionState_ = normalState_.get();
}

void Player::InitializeComponents() {
	//// ManaComponentの初期化 ////
	mana_ = std::make_unique<ManaComponent>(100.0f);

}
///////////////////
///
///   移動関係
///
///////////////////
void Player::AddForce(const Vector3& force) {
	externalVelocity_ += force;
}

void Player::Jump() {
	if (inputData_.isJump) {
		// Y軸に上向きの初速（ジャンプ力）を与える！
		float jumpPower = 7.0f; // 調整
		externalVelocity_.y = jumpPower;

		// ステートを「空中」に切り替える！
		ChangeMovementState(airborneState_.get());
	}
}