#pragma once
#include "Collider.h"
#include <list>
#include <vector>

// マップと動的な物体の当たり判定　と　動的な物体同士の当たり判定を作る
// ただし、

class CollisionManager
{
///////////////////////////
/// 
///  共通処理
///
///////////////////////////
public:
	// 前のフレームで保存したColliderの情報を削除する
	void Begin();
	
private:
	// 大まかにAABBで当たり判定を取る
	bool CheckNarrowPhase(Collider* a, Collider* b, Vector3& outPush);
	// GJKアルゴリズムの本体
	bool GJK(Collider* a, Collider* b, Vector3& outPush);
	// me->YourType と other->MyType が同じか調べる
	bool CheckFilter(Collider* colA, Collider* colB);

///////////////////////////
/// 
///  動的なオブジェクト関連の情報
///
///////////////////////////
public:
	// 動的なもの同士の当たり判定
	void CheckAllCollisions();

private:
	// 登録するColliderの情報
	std::vector<Collider*> colliders_;

public:
	// 当たり判定を取りたいColliderを登録する
	void SetColliders(Collider* collider);

///////////////////////////
/// 
///  Map関連の情報
///
///////////////////////////
public:
	// マップとの当たり判定（動的オブジェクト同士の後に呼ぶ）
	void CheckMapCollisions();

private:
	// ゲームに一個しかないはずのマップの情報(複数個になったらドンマイ俺)
	BVH* map_ = nullptr;

public:
	// マップ用の三角形データをセットする
	void SetMap(BVH* map) { map_ = map; }
};
