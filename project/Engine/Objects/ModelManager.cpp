#include "ModelManager.h"
#include <sstream>
#include "Log.h"
#include "Trigonometric.h"
#include "SkinningManager.h"
#include "StringPool.h"
#include "MathUtils.h"

namespace fs = std::filesystem;

void ModelManager::Initialize(Fngine* fngine) {
	pFngine_ = fngine;
}

//   =====================
// 【 ModelData保存系関数 】
//   =====================

uint32_t ModelManager::LoadObj(const std::string& filename, const std::string& directoryPath, LoadFileType type) {
	// Model Name を生成
	std::string modelName = filename;
	// ファイルの名前から.以降の拡張子を削除
	remove_extension_in_place(modelName);
	
	uint32_t id = StringPool::GetInstance()->GetStringID(modelName);

	// 二重ロード防止：すでに登録済みならそのままIDを返す
	if (id < models_.size() && models_[id] != nullptr) {
		return id;
	}

	// model を生成
	std::unique_ptr<ModelObject>model = std::make_unique<ModelObject>();
	// Model 初期化 -この処理はObjファイルのみになってしまっている
	model->Initialize(pFngine_->GetD3D12System(),filename, directoryPath,type);
	
	// -------------------------------------------------------------
	// モデルの物理三角形を抽出し、静的BVHを構築・保存する
	// -------------------------------------------------------------
	const auto& modelData = model->GetModelData();
	
	// スキニングデータが存在する場合、SkinningManager に静的データを自動登録！
	if (!modelData.skinClusterData.empty()) {
		SkinningManager::GetInstance()->RegisterFromModelData(
			id,
			modelData.rootNode,
			modelData.skinClusterData,
			static_cast<uint32_t>(modelData.vertices.size())
		);
	}

	// BVHを構築
	auto triangles = ExtractPhysicsTriangles(modelData.vertices, modelData.indices, Matrix4x4::Make::Identity());
	if (!triangles.empty()) {
		auto bvh = std::make_unique<BVH>();
		bvh->Build(triangles);

		EnsureCapacity(bvhs_, id);
		bvhs_[id] = std::move(bvh); // IDをキーにして保存
		Log::View("BVH Built successfully for Model: " + modelName);
	}
	
	// 追加
	EnsureCapacity(models_, id);
	models_[id] = std::move(model);
	// 返す
	return id;
}

void ModelManager::LoadAllModels(const std::string& baseDirectory) {
	if (!fs::exists(baseDirectory) || !fs::is_directory(baseDirectory)) {
		Log::View("[ModelManager] Directory not found: " + baseDirectory + "\n");
		return;
	}

	Log::View("=== LoadAllModels Started [" + baseDirectory + "] ===\n");

	// ディレクトリ内をサブフォルダ含めて再帰的に走査
	for (const auto& entry : fs::recursive_directory_iterator(baseDirectory)) {
		if (!entry.is_regular_file()) continue;

		const auto& path = entry.path();
		// 拡張子を取得
		std::string ext = path.extension().string();
		// 拡張子を小文字に統一
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		// ロード対象の拡張子フィルター
		if (ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".fbx") {
			std::string filename = path.filename().string();
			std::string directoryPath = path.parent_path().string();
			LoadModelFromFile(filename, directoryPath);
			Log::View("[ModelManager] Auto Loaded: " + path.string() + "\n");
		}
	}

	Log::View("=== LoadAllModels Completed ===\n");
}

void ModelManager::AddObject(const std::string& ID, const std::vector<VertexData>& vertices, const std::vector<uint32_t>& indices) {
	uint32_t id = StringPool::GetInstance()->GetStringID(ID);
	
	if (id < objects_.size() && objects_[id] != nullptr) {
		Log::View("The : " + ID + "Object -> Already Exists\n");
		return;
	}
	
	Log::View("The : " + ID + "Object -> Not already Exists\n");
	// 生成
	std::unique_ptr<ObjectData>object = std::make_unique<ObjectData>();
	object->MakeObjectData(pFngine_, vertices, indices);
	// 登録
	EnsureCapacity(objects_, id);
	objects_[id] = std::move(object);

	if (id < bvhs_.size() && bvhs_[id] != nullptr) {
		Log::View("The : " + ID + "BVH -> Already Exists\n");
		return;
	}
	else {
		// -------------------------------------------------------------
	// AddObject で追加された形状からも BVH を構築して保存する
	// -------------------------------------------------------------
		auto triangles = ExtractPhysicsTriangles(vertices, indices, Matrix4x4::Make::Identity());
		if (!triangles.empty()) {
			auto bvh = std::make_unique<BVH>();
			bvh->Build(triangles);

			EnsureCapacity(bvhs_, id);
			bvhs_[id] = std::move(bvh);
			Log::View("BVH Built successfully for Object: " + ID);
		}
	}
}

void ModelManager::LoadModelFromFile(const std::string& filename, const std::string& directoryPath) {
	// Model Name を生成
	std::string ID = filename;
	// 拡張子を削除
	remove_extension_in_place(ID);

	uint32_t id = StringPool::GetInstance()->GetStringID(ID);

	// 二重ロード防止：すでに登録済みならそのままIDを返す
	if (id < objects_.size() && objects_[id] != nullptr) {
		Log::View("The : " + ID + "Object -> Already Exists\n");
		return;
	}
	else {
		Log::View("The : " + ID + "Object -> Not already Exists\n");
	}
	
	/////////////////
	// ファイルをひらく
	/////////////////
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_FlipWindingOrder | aiProcess_FlipUVs);
	
	// ファイルが読み込めたか？メッシュが存在するかチェック
	assert(scene && scene->HasMeshes());

	/////////////////
	// ModelDataを構築する
	/////////////////]

	ModelData modelData;

	// ----------------------------
	// Node Data を構築 Skinning登録の際に使うから先
	// ----------------------------
	modelData.rootNode = ReadNode(scene->mRootNode);

	// メッシュデータの解析
	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		aiMesh* mesh = scene->mMeshes[meshIndex];

		// メッシュの開始頂点Indexを保持
		uint32_t vertexStart = static_cast<uint32_t>(modelData.vertices.size());

		// 頂点dataの追加
		for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
			aiVector3D position = mesh->mVertices[vertexIndex];
			aiVector3D normal = mesh->HasNormals() ? mesh->mNormals[vertexIndex] : aiVector3D(0.0f, 1.0f, 0.0f);
			aiVector3D texcoord = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][vertexIndex] : aiVector3D(0.0f, 0.0f, 0.0f);

			VertexData v;
			v.position = { -position.x, position.y, position.z, 1.0f };
			v.normal = { -normal.x, normal.y, normal.z };
			v.texcoord = { texcoord.x, texcoord.y };
			modelData.vertices.push_back(v);
		}

		// IndexDataの取得
		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
			aiFace& face = mesh->mFaces[faceIndex];
			assert(face.mNumIndices == 3);

			for (uint32_t element = 0; element < face.mNumIndices; ++element) {
				modelData.indices.push_back(vertexStart + face.mIndices[element]);
			}
		}

		// SkinningDataの取得
		for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
			aiBone* bone = mesh->mBones[boneIndex];
			std::string jointName = bone->mName.C_Str();
			JointWeightData& jointWeightData = modelData.skinClusterData[jointName];

			aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse();
			aiVector3D scale, translate;
			aiQuaternion rotate;
			bindPoseMatrixAssimp.Decompose(scale, rotate, translate);
			Matrix4x4 bindPoseMatrix = MakeAffineMatrix(
				{ scale.x, scale.y, scale.z },
				{ rotate.x, -rotate.y, -rotate.z, rotate.w },
				{ -translate.x, translate.y, translate.z }
			);
			jointWeightData.inverseBindPoseMatrix = Matrix4x4::Inverse(bindPoseMatrix);

			for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
				jointWeightData.vertexWeights.push_back({
					bone->mWeights[weightIndex].mWeight,
					vertexStart + bone->mWeights[weightIndex].mVertexId // オフセット加算
				});
			}
		}
	}


	// VertexとIndexを使ってObjectDataを構築
	std::unique_ptr<ObjectData>object = std::make_unique<ObjectData>();
	object->MakeObjectData(pFngine_, modelData.vertices, modelData.indices);
	// 登録
	EnsureCapacity(objects_, id);
	objects_[id] = std::move(object);

	// SkinningManagerに登録
	// スキニングデータが存在する場合、SkinningManager に静的データを自動登録！
	if (!modelData.skinClusterData.empty()) {
		SkinningManager::GetInstance()->RegisterFromModelData(
			id,
			modelData.rootNode,
			modelData.skinClusterData,
			static_cast<uint32_t>(modelData.vertices.size())
		);
	}

	// BVHを構築
	auto triangles = ExtractPhysicsTriangles(modelData.vertices, modelData.indices, Matrix4x4::Make::Identity());
	if (!triangles.empty()) {
		auto bvh = std::make_unique<BVH>();
		bvh->Build(triangles);

		EnsureCapacity(bvhs_, id);
		bvhs_[id] = std::move(bvh);
		Log::View("BVH Built successfully for Object: " + ID);
	}
}

Node ModelManager::ReadNode(aiNode* node) {
	// 返す値
	Node result;
	// 取得する値
	aiVector3D scale, translate;
	aiQuaternion rotate;
	// 取得開始
	// [ assimpの行列からSRTを抽出する関数を利用 ]
	node->mTransformation.Decompose(scale, rotate, translate);
	// [ 右手座標系から左手座標系に変換しながら代入 ]
	result.transform.Initialize();
	result.transform.set_.Scale({ scale.x,scale.y,scale.z });
	result.transform.set_.Quaternion({ rotate.x,-rotate.y,-rotate.z ,rotate.w });
	result.transform.set_.Translation({ -translate.x,translate.y ,translate.z });
	result.transform.LocalToWorld();
	// Node名を取得
	result.name = node->mName.C_Str();
	// 子供の数を取得し、サイズを確保
	result.children.resize(node->mNumChildren);
	// 読み込む
	for (uint32_t childIndex = 0;childIndex < node->mNumChildren;++childIndex) {
		// 再帰的に読み込んでいく
		result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
	}
	return result;
}


//   ==========================
// 【 データの海から取得系関数 】
//   ==========================

ModelData& ModelManager::GetModelData(uint32_t id) {
	if (id < models_.size() && models_[id]) {
		return models_[id]->GetModelData();
	}
	Log::ViewFile("Not Found Model Data for ID: " + std::to_string(id) + "\n");
	static ModelData dummy;
	return dummy;
}

ObjectData& ModelManager::GetObjectData(uint32_t id) {
	if (id < objects_.size() && objects_[id]) {
		return *objects_[id];
	}
	Log::ViewFile("Not Found Object Data for ID: " + std::to_string(id) + "\n");
	static ObjectData dummy;
	return dummy;
}

const BVH* ModelManager::GetBVH(uint32_t id) const {
	if (id < bvhs_.size() && bvhs_[id]) {
		return bvhs_[id].get();
	}
	return nullptr;
}

// 文字列互換関数
ModelData& ModelManager::LoadModelData(const std::string& ID) {
	return GetModelData(StringPool::GetInstance()->GetStringID(ID));
}

ObjectData& ModelManager::LoadObjectData(const std::string& ID) {
	return GetObjectData(StringPool::GetInstance()->GetStringID(ID));
}

const BVH* ModelManager::GetBVH(const std::string& ID) const {
	return GetBVH(StringPool::GetInstance()->GetStringID(ID));
}

//   ===============
// 【 BVH生成系関数 】
//   ===============

std::vector<PhysicsTriangle> ModelManager::ExtractPhysicsTriangles(const std::vector<VertexData>& verticesData, const std::vector<uint32_t>& indicesData, const Matrix4x4& matData) {
	// ポリゴンの情報
	std::vector<PhysicsTriangle> triangles;
	// 頂点情報
	const auto& vertices = verticesData;
	const auto& indices = indicesData;
	// ワールド行列
	const Matrix4x4& worldMat = matData;

	// IndexBufferがある場合（インデックス付きメッシュ）
	if (!indices.empty()) {
		for (size_t i = 0; i < indices.size(); i += 3) {
			PhysicsTriangle tri;

			// 計算負荷を下げるため、Transform（行列の掛け算）は1頂点につき1回だけ行う
			auto pos0 = Matrix4x4::Transform(worldMat, vertices[indices[i]].position);
			auto pos1 = Matrix4x4::Transform(worldMat, vertices[indices[i + 1]].position);
			auto pos2 = Matrix4x4::Transform(worldMat, vertices[indices[i + 2]].position);

			tri.v0 = { pos0.x, pos0.y, pos0.z };
			tri.v1 = { pos1.x, pos1.y, pos1.z };
			tri.v2 = { pos2.x, pos2.y, pos2.z };

			// ポリゴンに番号を振る
			tri.originalIndex = static_cast<int>(i / 3);

			triangles.push_back(tri);
		}
	}
	// IndexBufferがない場合（頂点が直接並んでいるトライアングルスープ）
	else if (!vertices.empty()) {
		// 頂点が3つずつのセットになっている前提でループを回す
		for (size_t i = 0; i + 2 < vertices.size(); i += 3) {
			PhysicsTriangle tri;

			auto pos0 = Matrix4x4::Transform(worldMat, vertices[i].position);
			auto pos1 = Matrix4x4::Transform(worldMat, vertices[i + 1].position);
			auto pos2 = Matrix4x4::Transform(worldMat, vertices[i + 2].position);

			tri.v0 = { pos0.x, pos0.y, pos0.z };
			tri.v1 = { pos1.x, pos1.y, pos1.z };
			tri.v2 = { pos2.x, pos2.y, pos2.z };

			// ポリゴンに番号を振る
			tri.originalIndex = static_cast<int>(i / 3);

			triangles.push_back(tri);
		}
	}

	// 最後にまとめて返す（これでif文の条件から外れても安全）
	return triangles;
}

std::vector<std::string> ModelManager::GetBVHNames() const {
	std::vector<std::string> names;
	for (size_t id = 0; id < bvhs_.size(); ++id) {
		if (bvhs_[id]) {
			names.push_back(StringPool::GetInstance()->GetStringFromID(static_cast<uint32_t>(id)));
		}
	}
	return names;
}