#include "Ground.h"

void Ground::Initialize(Fngine* fngine) {
	obj_ = std::make_unique<ModelObject>();
	obj_->modelName_ = "ground";
	obj_->textureName_ = "Legends_Ground";
	obj_->Initialize(fngine);

	// =============================
	// 【 当たり判定についての処理 】
	// =============================
	collider_ = std::make_unique<ConvexCollider>();

	// 属性の設定（自分は地面、当たる相手はプレイヤーや敵）
	collider_->SetMyType(COL_Ground);
	collider_->SetYourType(COL_Player | COL_Enemy);

	// 地面の頂点データ（横幅100、高さ1の長方形）
	// GJKが2D計算なので、XY平面上に作ります
	std::vector<Vector3> groundVertices = {
		{-50.0f, -5.0f, 0.0f}, // 左下
		{ 50.0f, -5.0f, 0.0f}, // 右下
		{-50.0f,  0.0f, 0.0f}, // 左上
		{ 50.0f,  0.0f, 0.0f}  // 右上
	};
	collider_->SetVertices(groundVertices);

	// 地面の位置（原点の少し下あたりに配置）
	collider_->SetWorldPosition({ 0.0f, 2.0f, 0.0f });

	// 持ち主を登録
	collider_->SetUserData(this);

	// 当たった時のテスト処理
	collider_->onCollisionCallback = [this](Collider* other, const Vector3& pushOut) {
		if (other->GetMyType() == COL_Player) {
			// プレイヤーが地面に触れたらログを出す（確認用）
			// printf("プレイヤーが地面に着地！\n");
		}
	};
}

void Ground::Update() {
	int index = 0;
	std::vector<Vector3> vertices = collider_->GetVertices();
	for (auto& vertex : vertices) {
		std::string string = std::to_string(index);
		ImGui::DragFloat3(("vertex : Pos" + string).c_str(), &vertex.x);
		index++;
	}
	collider_->SetVertices(vertices);

	Vector3 y = obj_->worldTransform_.get_.Translation();
	ImGui::DragFloat("Ground Pos", &y.y);
	obj_->worldTransform_.set_.Translation(y);
	collider_->SetWorldPosition(y);
	obj_->LocalToWorld();
	collider_->SetWorldMatrix(obj_->worldTransform_.mat_);
	collider_->UpdateAABB();
}

void Ground::Draw() {
	obj_->SetWVPData(CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(obj_->worldTransform_.mat_));
	obj_->Draw();
}














void ConvenienceModel::Initialize(Fngine* fngine,const std::string& modelName,const std::string& textureName) {
	obj_ = std::make_unique<ModelObject>();
	obj_->modelName_ = modelName.c_str();
	obj_->textureName_ = textureName.c_str();
	obj_->Initialize(fngine);
}

void ConvenienceModel::Update() {
#ifdef USE_IMGUI
	if (ImGui::TreeNode(obj_->modelName_.c_str())) {
		ImGui::DragFloat3("Scale", &obj_->worldTransform_.transform_.scale_.x);
		ImGui::DragFloat3("Rotate", &obj_->worldTransform_.transform_.rotation_.x);
		ImGui::DragFloat3("Pos", &obj_->worldTransform_.transform_.translation_.x);
		ImGui::TreePop();
	}
#endif//USEIMGUI
}

void ConvenienceModel::Draw() {
	obj_->worldTransform_.isDirty_ = true;
	obj_->LocalToWorld();
	obj_->SetWVPData(CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(obj_->worldTransform_.mat_));
	obj_->Draw();
}

void AnimModel::Initialize(Fngine* fngine, const std::string& modelName, const std::string& textureName, const std::string& filePath, const std::string& fileName) {
	obj_ = std::make_unique<ModelObject>();
	obj_->modelName_ = modelName.c_str();
	obj_->textureName_ = textureName.c_str();
	obj_->Initialize(fngine);

	animation_ = std::make_unique<Animation>();
	animation_->LoadAnimationFile(filePath.c_str(), fileName.c_str());
	animation_->SetIsLoop(true);

	skeleton_ = std::make_unique<Skeleton>();
	skeleton_->CreateSkeleton(obj_->GetNode());

	obj_->skinCluster_.Create(fngine, *skeleton_, obj_->GetModelData());
}

void AnimModel::Update() {
	obj_->worldTransform_.isDirty_ = true;
	animation_->TimeFlow();
	animation_->ApplyAnimation(*skeleton_.get());
	skeleton_->Update();
	obj_->skinCluster_.Update(*skeleton_);
}

void AnimModel::DrawImGui() {
#ifdef USE_IMGUI
	if (ImGui::TreeNode(obj_->modelName_.c_str())) {
		ImGui::DragFloat3("Scale", &obj_->worldTransform_.transform_.scale_.x);
		ImGui::DragFloat3("Rotate", &obj_->worldTransform_.transform_.rotation_.x);
		ImGui::DragFloat3("Pos", &obj_->worldTransform_.transform_.translation_.x);
		for (int i = 0; i < 4; ++i) {
			ImGui::DragFloat4(
				(std::string("Matrix") + " [" + std::to_string(i) + "]").c_str(),
				&obj_->worldTransform_.mat_.m[i][0]
			);
		}
		for (int i = 0; i < 4; ++i) {
			ImGui::DragFloat4(
				(std::string("Skeleton") + " [" + std::to_string(i) + "]").c_str(),
				&skeleton_->joints_[0].skeletonSpaceMatrix.m[i][0]
			);
		}
		ImGui::TreePop();
	}
#endif//USE_IMGUI
}

void AnimModel::Draw() {
	obj_->worldTransform_.isDirty_ = true;
	obj_->LocalToWorld();
	Matrix4x4 result = obj_->worldTransform_.mat_;
	if (skeleton_ && !skeleton_->joints_.empty()) {
		for (const auto& joint : skeleton_->joints_) {
			result = Matrix4x4::Multiply(result, joint.skeletonSpaceMatrix);
		}
	}
	obj_->worldTransform_.mat_ = result;
	obj_->SetWVPData(CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(obj_->worldTransform_.mat_));
	obj_->Draw();
}

void Anim2Model::Initialize(Fngine* fngine, const std::string& modelName, const std::string& textureName, const std::string& filePath, const std::string& fileName) {
	obj_ = std::make_unique<ModelObject>();
	obj_->modelName_ = modelName.c_str();
	obj_->textureName_ = textureName.c_str();
	obj_->Initialize(fngine);

	animation_ = std::make_unique<Animation>();
	animation_->LoadAnimationFile(filePath.c_str(), fileName.c_str());
	animation_->SetIsLoop(true);

	skeleton_ = std::make_unique<Skeleton>();
	skeleton_->CreateSkeleton(obj_->GetNode());

	obj_->skinCluster_.Create(fngine, *skeleton_, obj_->GetModelData());
}

void Anim2Model::Update() {
	obj_->worldTransform_.isDirty_ = true;
	animation_->TimeFlow();
	animation_->ApplyAnimation(*skeleton_.get());
	skeleton_->Update();
	obj_->skinCluster_.Update(*skeleton_);
}

void Anim2Model::DrawImGui() {
#ifdef USE_IMGUI
	if (ImGui::TreeNode(obj_->modelName_.c_str())) {
		ImGui::DragFloat3("Scale", &obj_->worldTransform_.transform_.scale_.x);
		ImGui::DragFloat3("Rotate", &obj_->worldTransform_.transform_.rotation_.x);
		ImGui::DragFloat3("Pos", &obj_->worldTransform_.transform_.translation_.x);
		for (int i = 0; i < 4; ++i) {
			ImGui::DragFloat4(
				(std::string("Matrix") + " [" + std::to_string(i) + "]").c_str(),
				&obj_->worldTransform_.mat_.m[i][0]
			);
		}
		for (int i = 0; i < 4; ++i) {
			ImGui::DragFloat4(
				(std::string("Skeleton") + " [" + std::to_string(i) + "]").c_str(),
				&skeleton_->joints_[0].skeletonSpaceMatrix.m[i][0]
			);
		}
		ImGui::TreePop();
	}
#endif//USE_IMGUI
}

void Anim2Model::Draw() {
	obj_->worldTransform_.isDirty_ = true;
	obj_->LocalToWorld();
	obj_->SetWVPData(CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(obj_->worldTransform_.mat_));
	obj_->Draw(ObjectDrawType::Animation);
}