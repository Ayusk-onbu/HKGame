#pragma once
#include "Collider.h"

class BossEnemy; // 前方宣言

// ボス本体の当たり判定を担うクラス
//class BossBodyCollider final :
//	public Collider
//{
//public:
//	BossBodyCollider(BossEnemy* boss);
//
//	//// 衝突が発生した際の処理
//	//void OnCollision(Collider* other) override;
//
//	//// Collider 継承の必須メソッド
//	//const Vector3 GetWorldPosition() override;
//
//private:
//	BossEnemy* boss_; // 親となるBossEnemyへのポインタ
//
//	// ボスが受けるダメージ量をここで定数として定義することも可能
//	const float BASE_DAMAGE_TAKEN = 5.0f;
//};

