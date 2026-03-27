#include "CollisionManager.h"
#include "GJK.h"

void CollisionManager::Begin() {
	colliders_.clear();
}

void CollisionManager::SetColliders(Collider* collider) {
	colliders_.push_back(collider);
}

/////////////////////////
///
///  メインシステム
///
/////////////////////////
void CollisionManager::CheckAllCollisions() {

	// 事前準備: 全コライダーのAABBを最新の座標に合わせて更新しておく
	for (auto* col : colliders_) {
		col->UpdateAABB();
	}

	// 総当たりで判定（ここでは簡略化して二重ループ）
	for (size_t i = 0; i < colliders_.size(); ++i) {
		Collider* colA = colliders_[i];
		for (size_t j = i + 1; j < colliders_.size(); ++j) {

			Collider* colB = colliders_[j];

			// フィルターチェック (MyType と YourType のビット演算など)
			if (!CheckFilter(colA, colB)) continue;

			// ==========================================
			// 【第1段階】ブロードフェーズ (AABB判定)
			// ==========================================
			if (AABB::IsHitAABB2AABB(colA->GetAABB(), colB->GetAABB())){
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
	if (a->GetShapeType() == ColliderShape::Convex &&
		b->GetShapeType() == ColliderShape::Convex)
	{
		// 両方ConvexならGJKアルゴリズムを実行！
		return GJK(static_cast<ConvexCollider*>(a), static_cast<ConvexCollider*>(b), outPush);
	}

	// Sphere vs Convex などの他の組み合わせもここに書く
	return false;
}

// GJKアルゴリズムの本体
bool CollisionManager::GJK(ConvexCollider* a, ConvexCollider* b, Vector3& outPush) {
	// GJKの実装（ミンコフスキー差とシンプレックスによる判定）
// 1. 各コライダーの頂点をワールド座標系(Vector2)に変換するための配列
	std::vector<Vector2> shapeA;
	std::vector<Vector2> shapeB;

	// コライダーAのワールド座標
	//Vector3 posA = a->GetWorldPosition();
	Matrix4x4 matA = a->GetWorldMatrix();
	for (const auto& v : a->GetVertices()) {
		Vector3 worldPos = Matrix4x4::Transform(v, matA);
		// ローカル座標にワールド座標を足して、Vector2に変換
		shapeA.push_back({ worldPos.x, worldPos.y });
	}

	// コライダーBのワールド座標
	//Vector3 posB = b->GetWorldPosition();
	Matrix4x4 matB = b->GetWorldMatrix();
	for (const auto& v : b->GetVertices()) {
		Vector3 worldPos = Matrix4x4::Transform(v, matB);
		shapeB.push_back({ worldPos.x, worldPos.y });
	}

	// 2. GJKの判定関数を呼び出す
	std::vector<SupportPoint> simplex;
	if (collision(shapeA, shapeB, simplex)) {
		
		Contact contact = EPA(shapeA, shapeB, simplex);

		outPush.x += contact.direction.x * contact.depth;
		outPush.y += contact.direction.y * contact.depth;
		outPush.z = 0.0f;

		return true; // 当たっている
	}

	return false; // 当たっていない
}