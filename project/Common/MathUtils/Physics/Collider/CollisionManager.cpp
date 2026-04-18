#include "CollisionManager.h"
#include "GJK.h"

void CollisionManager::Begin() {
	// Colliderの情報を初期化
	colliders_.clear();

	map_ = nullptr;
}

void CollisionManager::SetColliders(Collider* collider) {
	// Colliderを登録
	colliders_.push_back(collider);
}

/////////////////////////
///
///  メインシステム
///
/////////////////////////
void CollisionManager::CheckAllCollisions() {

	//   ======================================================================
	// 【 事前準備: 全動的なコライダーのAABBを最新の座標に合わせて更新しておく 】
	//   ======================================================================
	
	for (auto* col : colliders_) {
		col->UpdateAABB();
	}

	//   ================
	// 【 総当たりで判定 】
	//   ================
	for (size_t i = 0; i < colliders_.size(); ++i) {
		Collider* colA = colliders_[i];
		for (size_t j = i + 1; j < colliders_.size(); ++j) {

			Collider* colB = colliders_[j];

			// 万が一どちらかがnullptrならスキップ
			if (!colA || !colB) continue;

			// フィルターチェック (MyType と YourType のビット演算など)
			if (!CheckFilter(colA, colB)) continue;

			// ==========================================
			// 【第1段階】ブロードフェーズ (AABB判定)
			// ==========================================
			if (AABB::IsHitAABB2AABB(colA->GetAABB(), colB->GetAABB())) {
				// 押し出す量　※ 押し出す量を保存するため
				Vector3 pushOut = { 0.0f,0.0f,0.0f };

				// ==========================================
				// 【第2段階】ナローフェーズ (詳細判定)
				// ==========================================
				// AABB同士が重なっていたら、詳細な形状で判定する
				if (CheckNarrowPhase(colA, colB, pushOut))
				{
					// 最終的に当たっていたらコールバック呼び出し！
					colA->OnCollision(colB, pushOut);
					Vector3 pushOutB = { -pushOut.x, -pushOut.y, -pushOut.z };
					colB->OnCollision(colA, pushOutB);
				}
			}
		}
	}
}

//////////////////////
///
///  サポートシステム
///
//////////////////////
bool CollisionManager::CheckFilter(Collider* colliderA, Collider* colliderB) {
	if ((colliderA->GetMyType() & colliderB->GetYourType()) == 0 ||
		(colliderB->GetMyType() & colliderA->GetYourType()) == 0) {
		return false;
	}
	return true;
}

bool CollisionManager::CheckNarrowPhase(Collider* a, Collider* b, Vector3& outPush) {
	// ここで GetShapeType() を見て分岐する
	if (!a)return false;
	if (!b)return false;

	if (a->GetShapeType() == ColliderShape::Convex &&
		b->GetShapeType() == ColliderShape::Convex)
	{
		// 両方ConvexならGJKアルゴリズムを実行！
		return GJK(static_cast<MeshCollider*>(a), static_cast<MeshCollider*>(b), outPush);
	}

	// Sphere vs Convex などの他の組み合わせもここに書く
	return false;
}

// GJKアルゴリズムの本体
bool CollisionManager::GJK(Collider* a, Collider* b, Vector3& outPush) {

	std::vector<Vector3> simplex;
	if (GJK::_3D::GJK_GetSimplex(*a, *b, simplex)) {

		GJK::_3D::Contact3D contact = GJK::_3D::EPA(*a, *b, simplex);
		if (contact.hasCollision) {
			outPush = contact.normal * (contact.depth + 0.0001f);
			return true;
		}
	}
	return false; // 当たっていない
}

void CollisionManager::CheckMapCollisions() {
	if (!map_) return;

	// 動くオブジェクト（プレイヤーや敵など）すべてに対してループ
	for (Collider* dynamicCol : colliders_) {

		// 相手がGroundだったら判定しない
		if ((dynamicCol->GetMyType() == COL_Ground) ||
			(dynamicCol->GetMyType() == COL_Static_Map)) continue;

		dynamicCol->UpdateAABB();

		std::vector<PhysicsTriangle>triangles;

		map_->QueryTriangles(dynamicCol->GetAABB(), triangles);

		if (triangles.empty())continue;

		// 1. Broad-Phase: マップの全三角形とAABBで事前チェック
		for (const PhysicsTriangle& tri : triangles) {
			AABB triAABB = tri.GetAABB();

			// AABB同士が当たっていなければGJKはスキップ（爆速化）
			if (!AABB::IsHitAABB2AABB(dynamicCol->GetAABB(), triAABB)) {
				continue;
			}

			dynamicCol->Update();

			// 2. Narrow-Phase: GJK-EPAで正確な判定と押し戻し計算
			TriangleCollider triCollider(tri);
			Vector3 outPush{ 0,0,0 };

			// dynamicCol (Mesh等) vs triCollider (Triangle) の異種格闘技！
			if (GJK(dynamicCol, &triCollider, outPush)) {
				// コールバック関数があれば呼ぶ
				dynamicCol->OnCollision(&triCollider, outPush);

				// 押し戻された結果、次の三角形との判定がおかしくならないようAABBを更新
				dynamicCol->UpdateAABB();
			}
		}
	}
}