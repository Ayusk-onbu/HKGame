#include "Character.h"

void Character::Initialize(Fngine* engine, std::string modelName, std::string textureName) {
	DynamicObject::Initialize(engine, modelName, textureName);

	InitializeStates();
	ChangeMovementState("Idle");
}

//   ==========
// 【 更新処理 】
//   ==========
void Character::Update(float deltaTime) {
	if (controller_) {
		input_ = controller_->GetCommandState(input_);
	}

	moveAmount_ = Vector3{ 0.0f, 0.0f, 0.0f };

	DynamicObject::Update(deltaTime);

	if (mm_currentState_) {
		mm_currentState_->Update(deltaTime);
	}
	if (ac_currentState_) {
		ac_currentState_->Update(deltaTime);
	}

	moveAmount_ = (myVelocity_ + externalVelocity_) * deltaTime;

	obj_->worldTransform_.set_.Translation(obj_->worldTransform_.get_.Translation() + moveAmount_);

	onGround_ = false;
}

void Character::Draw() {
	DynamicObject::Draw();
}


///////////////////////////
/// 
/// 状態に関する関数
///
//////////////////////////
void Character::ChangeMovementState(std::string stateName) {
	if (mm_currentState_) {
		mm_currentState_->Exit();
	}
	mm_currentState_ = nullptr; // Reset current state

	if (stateName == "GroundBase") {
		mm_currentState_ = mm_states_.groundBase_.get();
	}
	else if (stateName == "AirBase") {
		mm_currentState_ = mm_states_.airBase_.get();
	}
	else if (stateName == "Idle") {
		mm_currentState_ = mm_states_.idleBase_.get();
	}
	else if (stateName == "Walk") {
		mm_currentState_ = mm_states_.walk_.get();
	}
	else if (stateName == "Restricted") {
		mm_currentState_ = mm_states_.restrictedBase_.get();
	}

	if (mm_currentState_) {
		mm_currentState_->Enter();
	}
}

void Character::InitializeStates() {
	// Movement 関係の初期化
	mm_states_.groundBase_ = std::make_unique<CharacterState::Movement::GroundBase>();mm_states_.groundBase_->SetInfo(this);
	mm_states_.airBase_ = std::make_unique<CharacterState::Movement::AirBase>();mm_states_.airBase_->SetInfo(this);
	mm_states_.restrictedBase_ = std::make_unique<CharacterState::Movement::Restricted>();mm_states_.restrictedBase_->SetInfo(this);

	mm_states_.idleBase_ = std::make_unique<CharacterState::Movement::Idle>();mm_states_.idleBase_->SetInfo(this, mm_states_.groundBase_.get());
	mm_states_.walk_ = std::make_unique<CharacterState::Movement::Walk>();mm_states_.walk_->SetInfo(this, mm_states_.groundBase_.get());
}
