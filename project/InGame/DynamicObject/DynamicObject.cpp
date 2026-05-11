#include "DynamicObject.h"

void DynamicObject::Initialize(Fngine* engine, std::string modelName, std::string textureName) {
	//   ======================
	// 【 オブジェクトの初期化 】
	//   ======================
	obj_ = std::make_unique<ModelObject>();
	obj_->modelName_ = modelName;// 使用するModelの名前
	obj_->textureName_ = textureName;// 使用するTextureの名前
	obj_->Initialize(engine);
}

void DynamicObject::Update(float deltaTime) {
	status_.Update(deltaTime);
	// 死んだとき処理はどこに書こう
	// 重力処理：毎フレーム externalVelocity_ の Y軸を下向きに加速させる
	externalVelocity_.y -= gravity_ * deltaTime;

	// 落下速度の制限（ターミナルベロシティ）
	if (externalVelocity_.y < MAX_FALL_VELOCITY) {
		externalVelocity_.y = MAX_FALL_VELOCITY;
	}
}

void DynamicObject::Draw() {
	obj_->SetWVPData(CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(obj_->worldTransform_.mat_));
	obj_->Draw();
}

void DynamicObject::OnCollisionGround(Collider* other, const Vector3& outPush) {
	Vector3 actualPush = -outPush;
	Vector3 normal = actualPush;
	float len2{ Dot(actualPush, actualPush) };
	if (len2 > 0.0f) {
		normal = normal / sqrt(len2);
	}

	if(other->GetMyType() == COL_Static_Map) {
		currentGroundFriction_ = 0.8f;
	}
	else if (other->GetMyType() == COL_Static_Map) {
		currentGroundFriction_ = 0.2f;
	}
	else {
		// 他のものなら何もしない
		return;
	}
	
	obj_->worldTransform_.set_.Translation(obj_->worldTransform_.get_.Translation() + actualPush);

	// 足元に地面があるかのチェック
	if (normal.y > 0.8f) {
		onGround_ = true;
	}
	else {
		// 床として判定できない場合は摩擦、フラグ等をリセット
		currentGroundFriction_ = 0.0f;
	}
}