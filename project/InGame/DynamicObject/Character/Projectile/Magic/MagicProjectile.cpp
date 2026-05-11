#include "MagicProjectile.h"

void MagicProjectile::Initialize(Fngine* engine, std::string modelName, std::string textureName) {
	Character::Initialize(engine, modelName, textureName);

	collider_->onCollisionCallBack = [this](Collider* other, const Vector3& pushOut) {
		OnHit(other, pushOut);
	};
}

void MagicProjectile::InitializeInformation(MagicData magicData) {
	projData_ = magicData.projData;
	collider_ = std::make_unique<MeshCollider>();
}

void MagicProjectile::Update(float deltaTime) {
	Character::Update(deltaTime);

}

void MagicProjectile::Draw() {
	obj_->LocalToWorld();
	Character::Draw();
}

void MagicProjectile::OnHit(Collider* other, const Vector3& outPush) {
    if (other->GetMyType() & (COL_Static_Map | COL_Ground)) {
        Destroy();
        return;
    }

    auto* victim = static_cast<DynamicObject*>(other->GetUserData());
    if (!victim) return;

    // 例：相手が敵で、この弾が攻撃用ならダメージ
    if ((other->GetMyType() & COL_Enemy) && projData_.target == TargetType::Enemy) {
        victim->GetStatus().TakeDamage(projData_.power, projData_.element);
        // デバフ付与...
    }
    // 例：相手がプレイヤーで、この弾が味方へのバフ用なら回復やバフ
    else if ((other->GetMyType() & COL_Player) && projData_.target == TargetType::Self) {
        // ※回復処理（負のダメージなど）やバフ付与
        for (const auto& effect : projData_.statusEffects) {
            victim->GetStatus().AddEffect(effect);
        }
    }

    if (!projData_.isPiercing) Destroy(); // 貫通しないで消える
}

void MagicProjectile::Destroy() {
	status_.SetCurrentHp(0.0f); // HPを0にして死んだ状態にする
    // 弾のモデルを非表示にしたり、オブジェクトプールに返却する処理
}