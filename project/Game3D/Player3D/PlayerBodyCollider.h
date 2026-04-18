#pragma once
#include "Collider.h"

class Player3D;

// プレイヤー本体の当たり判定を担うクラス
//class PlayerBodyCollider final :
//	public Collider // Colliderクラスを継承
//{
//public:
//	// コンストラクタ: 親となるPlayer3Dのポインタを受け取る
//	PlayerBodyCollider(Player3D* player);
//
//	//  衝突が発生した際の処理
//	//void OnCollision(Collider* other) override;
//
//	//// Collider 継承の必須メソッド: 自身のワールド座標を返す
//	//const Vector3 GetWorldPosition() override;
//
//private:
//	Player3D* player_; // 親となるPlayer3Dへのポインタ
//
//	// 敵の攻撃から受ける基本ダメージ量
//	const float BASE_DAMAGE_TAKEN = 30.0f;
//};
