#include "GJK.h"

namespace GJK {
	namespace _3D {
		Vector3 Support(const Collider& colliderA, const Collider& colliderB, Vector3 direction)
		{
			return colliderA.FindFurthestPoint(direction)
				- colliderB.FindFurthestPoint(-direction);
		}

		bool SameDirection(const Vector3& direction, const Vector3& ao)
		{
			return Dot(direction, ao) > 0;
		}

		bool Line(Simplex& points, Vector3& direction)
		{
			Vector3 a = points[0];
			Vector3 b = points[1];

			Vector3 ab = b - a;
			Vector3 ao = -a;

			if (SameDirection(ab, ao)) {
				direction = CrossProduct(CrossProduct(ab, ao), ab);
			}

			else {
				points = { a };
				direction = ao;
			}

			return false;
		}

		bool Triangle(Simplex& points, Vector3& direction)
		{
			Vector3 a = points[0];
			Vector3 b = points[1];
			Vector3 c = points[2];

			Vector3 ab = b - a;
			Vector3 ac = c - a;
			Vector3 ao = -a;

			Vector3 abc = CrossProduct(ab, ac);

			if (SameDirection(CrossProduct(abc, ac), ao)) {
				if (SameDirection(ac, ao)) {
					points = { a, c };
					direction = CrossProduct(CrossProduct(ac, ao), ac);
				}

				else {
					return Line(points = { a, b }, direction);
				}
			}

			else {
				if (SameDirection(CrossProduct(ab, abc), ao)) {
					return Line(points = { a, b }, direction);
				}

				else {
					if (SameDirection(abc, ao)) {
						direction = abc;
					}

					else {
						points = { a, c, b };
						direction = -abc;
					}
				}
			}

			return false;
		}

		bool Tetrahedron(Simplex& points, Vector3& direction)
		{
			Vector3 a = points[0];
			Vector3 b = points[1];
			Vector3 c = points[2];
			Vector3 d = points[3];

			Vector3 ab = b - a;
			Vector3 ac = c - a;
			Vector3 ad = d - a;
			Vector3 ao = -a;

			Vector3 abc = CrossProduct(ab, ac);
			Vector3 acd = CrossProduct(ac, ad);
			Vector3 adb = CrossProduct(ad, ab);

			// abc面の法線が「外」を向くように調整 (dと逆側ならそのままでOK)
			if (Dot(abc, ad) > 0) abc = -abc;
			if (SameDirection(abc, ao)) {
				points = { a, b, c };
				direction = abc;
				return false;
			}

			// acd面の法線が「外」を向くように調整
			if (Dot(acd, ab) > 0) acd = -acd;
			if (SameDirection(acd, ao)) {
				points = { a, c, d };
				direction = acd;
				return false;
			}

			// adb面の法線が「外」を向くように調整
			if (Dot(adb, ac) > 0) adb = -adb;
			if (SameDirection(adb, ao)) {
				points = { a, d, b };
				direction = adb;
				return false;
			}

			return true;
		}

		bool NextSimplex(Simplex& points, Vector3& direction)
		{
			switch (points.size()) {
			case 2: return Line(points, direction);
			case 3: return Triangle(points, direction);
			case 4: return Tetrahedron(points, direction);
			}

			// never should be here
			return false;
		}

		std::pair<std::vector<FaceNormal>, size_t> GetFaceNormals(
			const std::vector<Vector3>& polytope,
			const std::vector<size_t>& faces)
		{
			std::vector<FaceNormal> normals;
			size_t minTriangle = 0;
			float minDistance = FLT_MAX;

			if (faces.empty()) return { normals, minTriangle };

			for (size_t i = 0; i + 2 < faces.size(); i += 3) {
				Vector3 a = polytope[faces[i]];
				Vector3 b = polytope[faces[i + 1]];
				Vector3 c = polytope[faces[i + 2]];

				Vector3 normal = CrossProduct(b - a, c - a);
				normal = Normalize(normal);

				float distance = Dot(normal, a);

				// 距離が負なら法線の向きを反転して正にする（法線は外側を向く）
				if (distance < 0.0f) {
					normal = -normal;
					distance = -distance;
				}

				normals.emplace_back(normal, distance);

				if (distance < minDistance) {
					minDistance = distance;
					minTriangle = i / 3;
				}
			}

			return { normals, minTriangle };
		}

		void AddIfUniqueEdge(
			std::vector<std::pair<size_t, size_t>>& edges,
			const std::vector<size_t>& faces,
			size_t a, size_t b)
		{
			// faces[a], faces[b] を実際の頂点インデックスとして扱う
			size_t va = faces[a];
			size_t vb = faces[b];
			auto reverseIt = std::find(edges.begin(), edges.end(), std::make_pair(vb, va));
			if (reverseIt != edges.end()) {
				// 逆向きの辺があれば消す（内部で共有辺はキャンセルされる）
				edges.erase(reverseIt);
			}
			else {
				edges.emplace_back(va, vb);
			}
		}

		std::pair<size_t, size_t> AddSupportToPolytope(
			std::vector<Vector3>& polytope,
			std::vector<size_t>& faces,
			const Vector3& support)
		{
			// 1) 全面法線を取得
			auto [normals, minFace] = GetFaceNormals(polytope, faces);

			// 2) support に面が向いているか調べ、向いている面を削除してエッジを収集する
			std::vector<std::pair<size_t, size_t>> uniqueEdges;
			std::vector<size_t> newFaces; // 残すべき面のインデックスリスト（再構築用）

			for (size_t tri = 0; tri < normals.size(); ++tri) {
				const FaceNormal& fn = normals[tri];
				// 面の法線が support の方向を向いている (内積 > 0) なら削除対象
				if (Dot(fn.normal, support) > 0.0f) {
					// この三角形のフェイス頂点のインデックス
					size_t f = tri * 3;
					// 各辺を uniqueEdges に追加（逆向きがあれば消える）
					AddIfUniqueEdge(uniqueEdges, faces, f + 0, f + 1);
					AddIfUniqueEdge(uniqueEdges, faces, f + 1, f + 2);
					AddIfUniqueEdge(uniqueEdges, faces, f + 2, f + 0);
					// この三角形は保持しない（スキップ）
				}
				else {
					// 保持する三角形は newFaces にコピー
					size_t f = tri * 3;
					newFaces.push_back(faces[f + 0]);
					newFaces.push_back(faces[f + 1]);
					newFaces.push_back(faces[f + 2]);
				}
			}

			// 置き換え
			faces.swap(newFaces);

			// 3) uniqueEdges を使って新しい三角形を作る
			size_t supportIndex = polytope.size();
			polytope.push_back(support);

			size_t startTriIndex = faces.size() / 3; // 追加される三角形の開始インデックス
			for (auto& e : uniqueEdges) {
				// e.first, e.second は頂点インデックス
				faces.push_back(static_cast<size_t>(e.first));
				faces.push_back(static_cast<size_t>(e.second));
				faces.push_back(supportIndex);
			}
			size_t triCount = uniqueEdges.size();

			return { startTriIndex, triCount };
		}

		bool GJK_GetSimplex(const Collider& colliderA, const Collider& colliderB, std::vector<Vector3>& outSimplex) {
			// 初期サポート
			Vector3 support = Support(colliderA, colliderB, Vector3(1, 0, 0));
			Simplex points;
			points.push_front(support);

			Vector3 direction = -support;
			int iterations = 0;
			while (iterations++ < 64) {
				support = Support(colliderA, colliderB, direction);
				if (Dot(support, direction) <= 0.0f) {
					return false; // 衝突なし
				}
				points.push_front(support);
				if (NextSimplex(points, direction)) {
					// 衝突。points に残っている点を outSimplex にコピーする
					outSimplex.clear();
					for (size_t i = 0; i < points.size(); ++i) {
						outSimplex.push_back(points[(int)i]);
					}
					return true;
				}
			}
			return false;
		}

		Contact3D EPA(const Collider& colliderA, const Collider& colliderB, const std::vector<Vector3>& initialSimplex) {
			// 初期多面体の頂点リストを作る
			std::vector<Vector3> polytope = initialSimplex;

			// simplex は通常 4 点になるはず (A,B,C,D)。faces の初期化は順序に注意
			// 初期 simplex の頂点順に依存するが、代表的な四面体の faces を作成
			std::vector<size_t> faces;
			if (polytope.size() == 4) {
				// faces: triangles (0,1,2), (0,3,1), (0,2,3), (1,3,2)
				faces = { 0,1,2, 0,3,1, 0,2,3, 1,3,2 };
			}
			else if (polytope.size() == 3) {
				// 三角形しかない場合は背面に一点を追加して四面体にする（原点方向に少し押し戻す）
				// 安全処理: 単純に原点方向に小さな点を追加
				Vector3 a = polytope[0], b = polytope[1], c = polytope[2];
				Vector3 d = (a + b + c) / 3.0f + Vector3{ 0.001f,0.001f,0.001f };
				polytope.push_back(d);
				faces = { 0,1,2, 0,3,1, 0,2,3, 1,3,2 };
			}
			else {
				// 想定外: 返却
				return Contact3D();
			}

			constexpr int kMaxEPAIterations = 64;
			constexpr float kTolerance = 0.0001f;

			for (int iter = 0; iter < kMaxEPAIterations; ++iter) {
				// faces -> 法線と距離の列を取得（GetFaceNormals は既実装）
				auto [normals, minFace] = GetFaceNormals(polytope, faces);
				if (normals.empty()) break;

				Vector3 minNormal = normals[minFace].normal;
				float minDistance = normals[minFace].distance;

				// サポートポイントを取る
				Vector3 support = Support(colliderA, colliderB, minNormal);
				float sDistance = Dot(minNormal, support);

				// 終了条件
				if (sDistance - minDistance < kTolerance) {
					// minNormal は既に外向き; penetration depth を返す
					return Contact3D(minNormal, minDistance);
				}

				// 多面体を拡張（AddSupportToPolytope は既実装）
				auto added = AddSupportToPolytope(polytope, faces, support);
				// AddSupportToPolytope は polytope と faces を更新する（上で実装済み）
				// 次ループで normals を再計算して続行
			}
			// 収束しなかった場合は失敗とみなす
			return Contact3D();
		}
	}
}