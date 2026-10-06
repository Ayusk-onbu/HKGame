#include "RigidBody.h"
#include "DynamicObject.h"

void RigidBody::DrawUI() {
#ifdef USE_IMGUI
	ImGui::Text("RigidBody Component");
	ImGui::Separator();
	// BodyTypeの選択
	const char* bodyTypeNames[] = { "Dynamic", "Kinematic", "Static" };
	int currentType = static_cast<int>(bodyType_);
	if (ImGui::Combo("Body Type", &currentType, bodyTypeNames, IM_ARRAYSIZE(bodyTypeNames))) {
		bodyType_ = static_cast<BodyType>(currentType);
	}
	// 重力の有効化
	ImGui::Checkbox("重力の有無", &useGravity_);
	// 反発係数と摩擦係数のスライダー
	ImGui::SliderFloat("反発係数", &restitution_, 0.0f, 1.0f);
	ImGui::SliderFloat("摩擦係数", &friction_, 0.0f, 1.0f);
	// 質量の入力
	ImGui::InputFloat("質量", &mass_);
	if (mass_ <= 0.0f) mass_ = 1.0f; // 質量は正の値に制限
	// 移動方向に向くかどうかのチェックボックス
	ImGui::Checkbox("移動方向に向くかどうか", &orientToMovement_);
	// 現在の速度を表示
	ImGui::Text("現在の速度: (%.2f, %.2f, %.2f)", myVelocity_.x, myVelocity_.y, myVelocity_.z);
    if(ImGui::Button("リセット速度")) {
        myVelocity_ = { 0.0f, 0.0f, 0.0f };
    }
#endif // USE_IMGUI
}

std::string RigidBody::GetComponentName() const {
    return "RigidBody";
}

json RigidBody::Serialize() const {
    json j;
    j["bodyType"] = static_cast<int>(bodyType_);
    j["useGravity"] = useGravity_;
    j["mass"] = mass_;
    j["restitution"] = restitution_;
    j["friction"] = friction_;
    j["orientToMovement"] = orientToMovement_;
    return j;
}

void RigidBody::Deserialize(const json& j) {
    if (j.contains("bodyType")) bodyType_ = static_cast<BodyType>(j["bodyType"].get<int>());
    if (j.contains("useGravity")) useGravity_ = j["useGravity"].get<bool>();
    if (j.contains("mass")) mass_ = j["mass"].get<float>();
    if (j.contains("restitution")) restitution_ = j["restitution"].get<float>();
    if (j.contains("friction")) friction_ = j["friction"].get<float>();
    if (j.contains("orientToMovement")) orientToMovement_ = j["orientToMovement"].get<bool>();
}

void RigidBody::Initialize() {
    myVelocity_ = { 0, 0, 0 };
    externalVelocity_ = { 0, 0, 0 };
    force_Accumulator_ = { 0, 0, 0 };
    isOnGround_ = false;
}

void RigidBody::AddForce(const Vector3& force) {
    if (bodyType_ != BodyType::Dynamic) return; // Dynamic以外は力を受けない
    force_Accumulator_ += force;
}

void RigidBody::Update(float deltaTime) {
    if (bodyType_ == BodyType::Static || !master_) return;

    // 接地フラグを毎フレーム初期化（この後の衝突判定で地面に触れていれば true になる）
    isOnGround_ = false;

    // タイプごとの速度更新
    if (bodyType_ == BodyType::Dynamic) {
        // 重力と外力の計算
        Vector3 acceleration = force_Accumulator_ / mass_;
        if (useGravity_) {
            acceleration += kGravity;
        }

        // 速度の更新 (v = v0 + a * t)
        myVelocity_ += acceleration * deltaTime;

        // 空気抵抗・地面の摩擦による減速
        myVelocity_.x *= (1.0f - friction_ * deltaTime);
        myVelocity_.z *= (1.0f - friction_ * deltaTime);

        // 力をクリア
        force_Accumulator_ = { 0, 0, 0 };
    }
    else if (bodyType_ == BodyType::Kinematic) {
        // Kinematic の場合、重力が有効かつ空中にいるなら自然落下を適用
        if (useGravity_ && !isOnGround_) {
            myVelocity_.y += kGravity.y * deltaTime;
        }
    }

    // 自身の速度 ＋ 外部速度 を合成して 1 フレームの移動量を算出
    Vector3 totalVelocity = myVelocity_ + externalVelocity_;
    Vector3 moveAmount = totalVelocity * deltaTime;

    myVelocity_ = { 0.0f,0.0f,0.0f };

    // Transform の座標を更新
    Vector3 currentPos = master_->GetTransform().get_.Translation();
    master_->SetPosition(currentPos + moveAmount);

    // 移動している方向（XZ平面）に向く（Orient to Movement）
    if (orientToMovement_) {
        Vector3 moveDirectionXZ = { moveAmount.x, 0.0f, moveAmount.z };
        if (LengthSquared(moveDirectionXZ) > 0.0001f) { // ゼロ除算・微小振動の防止
            moveDirectionXZ = Normalize(moveDirectionXZ);
            master_->GetTransform().LookAtToDirection(moveDirectionXZ);
        }
    }

    // 外部速度を少しずつ減衰させる（慣性減衰）
    externalVelocity_ *= (1.0f - 5.0f * deltaTime);
    if (LengthSquared(externalVelocity_) < 0.0001f) {
        externalVelocity_ = { 0, 0, 0 };
    }
}

// 衝突時に ColliderComponent や CollisionManager から呼ばれる応答処理
void RigidBody::ResolveCollision(const Vector3& pushOut) {
    if (bodyType_ == BodyType::Static || !master_) return;

    // 位置のめり込み補正（押し戻す）
    Vector3 currentPos = master_->GetTransform().get_.Translation();
    master_->SetPosition(currentPos + pushOut);

    // 上向きに押された＝地面に着地したと判定
    if (pushOut.y > 0.001f && myVelocity_.y < 0.0f) {
        isOnGround_ = true;
        myVelocity_.y = 0.0f; // 着地したら落下速度をリセット
    }

    // 押し戻し方向の速度成分を打ち消す（壁抜け・床抜け防止）
    Vector3 pushDir = Normalize(pushOut);
    float dot = Dot(myVelocity_, pushDir);

    if (dot < 0.0f) {
        if (bodyType_ == BodyType::Dynamic) {
            // 反発（跳ね返り）計算
            myVelocity_ -= pushDir * (dot * (1.0f + restitution_));
        }
        else if (bodyType_ == BodyType::Kinematic) {
            // Kinematic は反発させず、壁・床方向の速度を 0 にクリア
            myVelocity_ -= pushDir * dot;
        }
    }
}