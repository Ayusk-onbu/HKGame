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

	auto aabb = bvh_->GetRoot()->bounds;
	float lengthX = aabb.GetSize().x / 1.5f;
	myCollider->SetVertices({
		{aabb.min.x + lengthX, aabb.min.y, aabb.min.z}, // 左下
		{aabb.max.x - lengthX, aabb.min.y, aabb.min.z}, // 右下
		{aabb.max.x - lengthX, aabb.max.y, aabb.min.z}, // 右上
		{aabb.min.x + lengthX, aabb.max.y, aabb.min.z}, // 左上

		{aabb.min.x + lengthX, aabb.min.y, aabb.max.z}, // 左下
		{aabb.max.x - lengthX, aabb.min.y, aabb.max.z}, // 右下
		{aabb.max.x - lengthX, aabb.max.y, aabb.max.z}, // 右上
		{aabb.min.x + lengthX, aabb.max.y, aabb.max.z}, // 左上
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