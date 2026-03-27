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

		baseJoint_.SetType(AttachmentType::UmbrellaHandle);// 自分のタイプ
		baseJoint_.SetAcceptType(AttachmentType::PlayerHand | AttachmentType::PlayerBack);// 接続先のタイプ
		baseJoint_.SetInfo({0.0f,-0.5f,0.0f}, {0.0f,0.0f,0.0f});

		tipJoint_.SetType(AttachmentType::UmbrellaTip);
		tipJoint_.SetAcceptType(AttachmentType::UmbrellaTopRoot | AttachmentType::UmbrellaHandle);
		tipJoint_.SetInfo({ 0.0f,1.25f,0.0f }, { 0.0f,0.0f,0.0f });
		tipJoint_.AttachTo(&baseJoint_);
	}

	void Handle::Update(float deltaTime) {
		
		baseJoint_.Update();
		obj_->worldTransform_.mat_ = baseJoint_.GetMatrix();

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

		openObj_ = std::make_unique<ModelObject>();
		openObj_->textureName_ = "GridLine";
		openObj_->modelName_ = "UmbrellaTop";
		openObj_->Initialize(p_fngine_);

		rootJoint_.SetType(AttachmentType::UmbrellaTopRoot);// 自分のタイプ
		rootJoint_.SetAcceptType(AttachmentType::UmbrellaTip);// 接続可能なタイプ
		rootJoint_.SetInfo({ 0.0f,0.0f,0.0f }, { 0.0f,0.0f,0.0f });

		// ======================
		// 当たり判定
		// ======================
		collider_ = std::make_unique<ConvexCollider>();

		collider_->SetMyType(COL_None);
		collider_->SetYourType(COL_Enemy);

		collider_->SetUserData(this);

		collider_->onCollisionCallback = [](Collider* other, const Vector3& outPush) {

		};

		UpdateColliderShape();
	}

	void Top::Update(float deltaTime) {
		/*stateの更新処理*/
		if (currentState_) {
			currentState_->Update(deltaTime);
		}

		rootJoint_.Update();

		switch (form_) {
		case UmbrellaForm::Closed:
			obj_->worldTransform_.mat_ = rootJoint_.GetMatrix();
			break;
		case UmbrellaForm::Opened:
			openObj_->worldTransform_.mat_ = rootJoint_.GetMatrix();
			break;
		}

		if (collider_->GetMyType() != COL_None) {
			collider_->SetWorldPosition(rootJoint_.GetWorldPos());

			collider_->SetWorldMatrix(rootJoint_.GetMatrix());

			collider_->UpdateAABB();
		}

	}

	void Top::Draw() {
		switch (form_) {
		case UmbrellaForm::Closed:
			obj_->LocalToWorld();
			obj_->SetWVPData(CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(obj_->worldTransform_.mat_));
			obj_->Draw();
			break;
		case UmbrellaForm::Opened:
			openObj_->LocalToWorld();
			openObj_->SetWVPData(CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(openObj_->worldTransform_.mat_));
			openObj_->Draw();
			break;
		}
	}

	void Top::ChangeState(UmbrellaStates::Base* newState) {
		if (currentState_) {
			currentState_->Exit();
			delete currentState_;
		}
		currentState_ = newState;
		if (currentState_) {
			currentState_->SetTop(this);
			currentState_->Enter();
		}
	}

	void Top::UpdateColliderShape() {
		std::vector<Vector3> vertices;

		if (form_ == UmbrellaForm::Closed) {
			// 閉じた状態：細長い剣のような判定（ローカル座標で定義）
			// 幅0.2m、長さ1.5m(Y方向) の直方体の8頂点などを設定
			float w = 0.1f;
			float h = 1.5f;
			vertices = {
				{-w,  0.0f, -w}, { w,  0.0f, -w}, {-w,  0.0f,  w}, { w,  0.0f,  w}, // 根元
				{-w,     h, -w}, { w,     h, -w}, {-w,     h,  w}, { w,     h,  w}  // 先端
			};
		}
		else if (form_ == UmbrellaForm::Opened) {

		}
		else if (form_ == UmbrellaForm::Reverse) {

		}

		// ConvexCollider に頂点をセットする関数（無ければ Collider.h に追加してください）
		collider_->SetVertices(vertices);
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