#include "Player.h"

Player::Player() {
	controller_ = std::make_unique<PlayerController>();
}

void Player::Initialize(Fngine* fngine) {
	Character::Initialize(fngine, "Naira_ExportTest", "ulthimaSky");

	skeleton_ = std::make_unique<Skeleton>();
	skeleton_->CreateSkeleton(obj_->GetNode());

	obj_->skinCluster_.Create(fngine, *skeleton_, obj_->GetModelData());


	MeshCollider* myCollider = CreateCollider<MeshCollider>();
	myCollider->SetMyType(COL_Player);
	myCollider->SetYourType(COL_Static_Map);

	myCollider->SetVertices({
		{-1.0f, -0.5f, -0.5f}, // 左下
		{ 1.0f, -0.5f, -0.5f}, // 右下
		{ 1.0f,  0.8f, -0.5f},  // 右上
		{ -1.0f,  0.8f, -0.5f }, // 左上
		{-1.0f, -0.5f, 0.5f}, // 左下
		{ 1.0f, -0.5f, 0.5f}, // 右下
		{ 1.0f,  0.8f, 0.5f},  // 右上
		{ -1.0f,  0.8f, 0.5f }, // 左上
	});

	myCollider->onCollisionCallBack = [this](Collider* other, const Vector3& pushOut) {
		OnCollisionGround(other, pushOut);
	};
}

void Player::Update(float deltaTime) {
	Character::Update(deltaTime);

	skeleton_->Update();
	obj_->skinCluster_.Update(*skeleton_);

	collider_->SetWorldPosition(obj_->worldTransform_.get_.Translation());
	MeshCollider* meshCollider = dynamic_cast<MeshCollider*>(collider_.get());
	meshCollider->SetWorldMatrix(obj_->worldTransform_.mat_);
}

void Player::Draw() {
	obj_->LocalToWorld();
	Character::Draw();
}