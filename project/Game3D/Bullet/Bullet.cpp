#include "Bullet.h"
#include "CameraSystem.h"

//void BulletCollider::OnCollision(Collider* other)
//{
//	// 衝突相手がBulletの衝突マスクと一致しているか確認はCollisionManager側で行われる
//	// 衝突が発生したらBulletに死亡フラグを立てる
//	if (bullet_)
//	{
//		bullet_->Dead();
//	}
//}

void Bullet::Initialize(Fngine* fngine, const Vector3& pos, const Vector3& speed, int MyType, int yourType, float radius, float lifeTime) {
	fngine_ = fngine;
	obj_ = std::make_unique<ModelObject>();
	obj_->modelName_ = "cube";
	obj_->textureName_ = "GridLine";
	obj_->Initialize(fngine);
	obj_->worldTransform_.set_.Translation(pos);
	obj_->worldTransform_.set_.Scale({ radius,radius,radius });

	/*collider_ = std::make_unique<BulletCollider>(this);
	collider_->SetMyType(MyType);
	collider_->SetYourType(yourType);
	collider_->SetRadius(radius);
	collider_->SetWorldPosition(pos);*/

	velocity_ = speed;
	lifeTime_ = lifeTime;
	isDead_ = false;
}

void Bullet::Update() {
	if (isDead_) { return; }

	// 位置の更新
	Vector3 pos = obj_->worldTransform_.get_.Translation();
	pos += velocity_;
	obj_->worldTransform_.set_.Translation(pos);

	// コライダーの更新
	//collider_->SetWorldPosition(pos);

	// ライフタイムの更新
	lifeTime_ -= 1.0f / 60.0f;

	// 寿命による死亡
	if (lifeTime_ <= 0.0f) {
		// 死ぬ
		Dead();
	}
#ifdef USE_IMGUI
	ImGui::Begin("Bullet");
	ImGui::End();
#endif// USE_IMGUI
}

void Bullet::Draw() {
	obj_->LocalToWorld();
	obj_->SetWVPData(CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(obj_->worldTransform_.mat_));
	obj_->Draw();
}

void Bullet::Dead() {
	isDead_ = true;
	// 衝突時にコライダーを無効化し、それ以上の衝突判定を防ぐ
	/*collider_->SetMyType(COL_None);
	collider_->SetYourType(COL_None);*/
}