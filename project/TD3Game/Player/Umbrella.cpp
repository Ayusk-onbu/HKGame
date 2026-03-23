#include "Umbrella.h"

namespace Umbrella {

	////////////////////////
	///
	///  傘の「持ち手」の部分
	///
	///////////////////////
	void Handle::Initialize(Fngine* f) {
		obj_ = std::make_unique<ModelObject>();
		p_fngine_ = f;
		obj_->textureName_ = "GridLine";
		obj_->modelName_ = "UmbrellaHandle";
		obj_->Initialize(p_fngine_);

		baseJoint_.SetType(AttachmentType::UmbrellaHandle);
		baseJoint_.SetAcceptType(AttachmentType::PlayerHand | AttachmentType::PlayerBack);
		baseJoint_.SetInfo({0.0f,-0.5f,0.0f}, {0.0f,0.0f,0.0f});

		tipJoint_.SetAcceptType(AttachmentType::UmbrellaTopRoot);
		tipJoint_.SetType(AttachmentType::UmbrellaTip);
		tipJoint_.SetInfo({ 0.0f,0.5f,0.0f }, { 0.0f,0.0f,0.0f });
	}

	void Handle::Update(float deltaTime) {
		
		baseJoint_.Update();
		obj_->worldTransform_.mat_ = baseJoint_.GetMatrix();

		tipJoint_.SetPos({ obj_->worldTransform_.mat_.m[3][0],obj_->worldTransform_.mat_.m[3][1] + 1.25f ,obj_->worldTransform_.mat_.m[3][2]});
		tipJoint_.Update();
	}

	void Handle::Draw() {
		obj_->SetWVPData(CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(obj_->worldTransform_.mat_));
		obj_->Draw();
	}

	////////////////////////
	///
	///  傘の「かさ」の部分
	///
	///////////////////////
	void Top::Initialize(Fngine* f) {
		obj_ = std::make_unique<ModelObject>();
		p_fngine_ = f;
		obj_->textureName_ = "GridLine";
		obj_->modelName_ = "UmbrellaTopClose";
		obj_->Initialize(p_fngine_);

		rootJoint_.SetAcceptType(AttachmentType::UmbrellaTip);
		rootJoint_.SetType(AttachmentType::UmbrellaTopRoot);
		rootJoint_.SetInfo({ 0.0f,0.5f,0.0f }, { 0.0f,0.0f,0.0f });
	}

	void Top::Update(float deltaTime) {
		/*stateの更新処理*/
		if (currentState_) {
			currentState_->Update(deltaTime);
		}

		rootJoint_.Update();

		obj_->worldTransform_.mat_ = rootJoint_.GetMatrix();
	}

	void Top::Draw() {
		obj_->LocalToWorld();
		obj_->SetWVPData(CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(obj_->worldTransform_.mat_));
		obj_->Draw();
	}

	void Top::ChangeState(UmbrellaStates::Base* newState) {
		if (currentState_) {
			currentState_->Exit();
		}
		currentState_ = newState;
		if (currentState_) {
			currentState_->Enter();
		}
	}

	////////////////////////
	///
	///  傘の「メイン」の部分
	///
	///////////////////////
	void Main::Initialize(Fngine* f) {
		handle_ = std::make_unique<Handle>();
		top_ = std::make_unique<Top>();

		handle_->Initialize(f);
		top_->Initialize(f);

		top_->GetRootJoint()->AttachTo(handle_->GetTipJoint());
		top_->ChangeState(new UmbrellaStates::Attached());
	}

	void Main::Update(float deltaTime) {
		handle_->Update(deltaTime);
		top_->Update(deltaTime);
	}

	void Main::Draw() {
		handle_->Draw();
		top_->Draw();
	}
}