#include "BossBodyCollider.h"
#include "Enemy.h"

//BossBodyCollider::BossBodyCollider(BossEnemy* boss):boss_(boss) {
//	SetMyType(COL_Enemy);
//	SetYourType(COL_Player_Attack);
//}

//void BossBodyCollider::OnCollision(Collider* other)
//{
//	if (!boss_) return;
//
//	// 1. **衝突相手がプレイヤーの攻撃であるかチェック**
//	// プレイヤーの攻撃Colliderは COL_PLAYER_ATTACK 属性を持っていると仮定
//	if (other->GetMyType() & COL_Player_Attack)
//	{
//		// 既に死亡状態であればダメージ処理を行わない
//		if (boss_->IsDead()) {
//			return;
//		}
//
//		// 2. **ダメージ値を決定**
//
//		// 💡 (A案) 衝突相手からダメージ値を取得
//		// PlayerAttackCollider* playerAttack = dynamic_cast<PlayerAttackCollider*>(other);
//		// float damage = playerAttack ? playerAttack->GetDamageValue() : BASE_DAMAGE_TAKEN;
//
//		// 💡 (B案) 固定値で処理 (今回はシンプルにこちらを採用)
//		float damage = BASE_DAMAGE_TAKEN;
//
//		// 3. **BossEnemy 本体にダメージを通知**
//		boss_->TakeDamage(damage);
//
//		// 4. (必要であれば) 衝突相手の攻撃判定を無効化する処理など
//	}
//}
//
//const Vector3 BossBodyCollider::GetWorldPosition()
//{
//	// ボスの世界座標を返す (WorldTransform経由)
//	return boss_->GetPosition();
//}