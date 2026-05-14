#include "ModelManager.h"
#include <sstream>
#include "Log.h"
#include "Trigonometric.h"
#include "MathUtils.h"

std::unique_ptr<ModelManager>ModelManager::instance_ = nullptr;

void ModelManager::Initialize(Fngine* fngine) {
	pFngine_ = fngine;
}

ObjectData& ModelManager::GetObjectData(const std::string& ID) {
	auto it = objects_.find(ID);

	// 見つからなかったらエラーを出して、プログラムを強制停止させる
	if (it == objects_.end()) {
		Log::ViewFile("Not Found Object Data: " + ID);
		assert(0 && "Not Found Object Data!");
	}

	// unique_ptrの中身を参照として返す
	return *(it->second);
}

void ModelManager::AddObject(const std::string name, const std::vector<VertexData>& vertices, const std::vector<uint32_t>& indices) {
	auto d3d12 = pFngine_->GetD3D12System();

	std::unique_ptr<ObjectData>object = std::make_unique<ObjectData>();
	object->GetVertexResource() = CreateBufferResource(d3d12.GetDevice().Get(), sizeof(VertexData) * vertices.size());

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	vertexBufferView.BufferLocation = object->GetVertexResource()->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * vertices.size());
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	object->SetVertexBufferView(vertexBufferView);

	object->GetVertexResource()->Map(0, nullptr, reinterpret_cast<void**>(&*object->GetVertexData()));
	std::memcpy(object->GetVertexData(), vertices.data(), sizeof(VertexData) * vertices.size());

	if (indices.size() > 0) {
		D3D12_INDEX_BUFFER_VIEW indexBufferView;

		indexBufferView.BufferLocation = object->GetIndexResource()->GetGPUVirtualAddress();
		indexBufferView.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * indices.size());
		indexBufferView.Format = DXGI_FORMAT_R32_UINT;

		object->SetIndexBufferView(indexBufferView);

		object->GetIndexResource()->Map(0, nullptr, reinterpret_cast<void**>(&*object->GetIndexData()));
		std::memcpy(object->GetIndexData(), indices.data(), sizeof(uint32_t) * indices.size());
	}

	// 追加
	objects_.emplace(name, std::move(object));
}

std::string ModelManager::LoadObj(const std::string& filename, const std::string& directoryPath, LoadFileType type) {
	auto d3d12 = pFngine_->GetD3D12System();

	// model を生成
	std::unique_ptr<ObjectData>object = std::make_unique<ObjectData>();

	ModelData modelData;

	// Model 初期化 -この処理はObjファイルのみになってしまっている
	if (type == LoadFileType::Assimp) {
		modelData = LoadFiles(filename, directoryPath);
		// Index Resource の作成
		object->GetIndexResource() = CreateBufferResource(d3d12.GetDevice().Get(), sizeof(uint32_t) * modelData.indices.size());
	}
	else if (type == LoadFileType::OBJ) {
		modelData = LoadObjFile(filename, directoryPath);
	}

	object->GetVertexResource() = CreateBufferResource(d3d12.GetDevice().Get(), sizeof(VertexData) * modelData.vertices.size());

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	vertexBufferView.BufferLocation = object->GetVertexResource()->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	object->SetVertexBufferView(vertexBufferView);

	object->GetVertexResource()->Map(0, nullptr, reinterpret_cast<void**>(&*object->GetVertexData()));
	std::memcpy(object->GetVertexData(), modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

	if (modelData.indices.size() > 0) {
		D3D12_INDEX_BUFFER_VIEW indexBufferView;

		indexBufferView.BufferLocation = object->GetIndexResource()->GetGPUVirtualAddress();
		indexBufferView.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * modelData.indices.size());
		indexBufferView.Format = DXGI_FORMAT_R32_UINT;

		object->SetIndexBufferView(indexBufferView);

		object->GetIndexResource()->Map(0, nullptr, reinterpret_cast<void**>(&*object->GetIndexData()));
		std::memcpy(object->GetIndexData(), modelData.indices.data(), sizeof(uint32_t) * modelData.indices.size());
	}

	// Model Name を生成
	std::string modelName = filename;
	// ファイルの名前から.以降の拡張子を削除
	remove_extension_in_place(modelName);
	// 追加
	objects_.emplace(modelName, std::move(object));
	// 返す
	return modelName;
}

ModelData ModelManager::LoadFiles(const std::string& fileName, const std::string& directoryPath) {
	/////////////////
	// 変数宣言
	/////////////////
	ModelData modelData;

	/////////////////
	// ファイルをひらく
	/////////////////
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + fileName;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_FlipWindingOrder | aiProcess_FlipUVs);
	assert(scene->HasMeshes());

	/////////////////
	// ModelDataを構築する
	/////////////////

	for (uint32_t meshIndex = 0;meshIndex < scene->mNumMeshes;++meshIndex) {
		aiMesh* mesh = scene->mMeshes[meshIndex];

		if (mesh->HasNormals() == false) {
			mesh->mNormals = new aiVector3D[mesh->mNumVertices];
		}
		assert(mesh->HasNormals());// 法線を持っているかのチェック

		if (!mesh->HasTextureCoords(0)) {
			mesh->mTextureCoords[0] = new aiVector3D[mesh->mNumVertices];
			for (uint32_t i = 0; i < mesh->mNumVertices; ++i) {
				mesh->mTextureCoords[0][i] = aiVector3D(0.0f, 0.0f, 0.0f);
			}
		}
		assert(mesh->HasTextureCoords(0));// Texcoordを持っているかのチェック
		// ここからmeshの中身（Face）の解析
		// 頂点数分のメモリを確保する
		modelData.vertices.resize(mesh->mNumVertices);
		for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices;++vertexIndex) {
			aiVector3D& position = mesh->mVertices[vertexIndex];
			aiVector3D& normal = mesh->mNormals[vertexIndex];
			aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];
			// 右手座標系から左手座標系に変換しながら保存
			modelData.vertices[vertexIndex].position = { -position.x,position.y, position.z ,1.0f };
			modelData.vertices[vertexIndex].normal = { -normal.x,normal.y,normal.z };
			modelData.vertices[vertexIndex].texcoord = { texcoord.x,texcoord.y };
		}

		// IndexDataの取得
		for (uint32_t faceIndex = 0;faceIndex < mesh->mNumFaces;++faceIndex) {
			aiFace& face = mesh->mFaces[faceIndex];
			assert(face.mNumIndices == 3);// 三角形のみを読み込むようになる

			for (uint32_t element = 0; element < face.mNumIndices; ++element) {
				uint32_t vertexIndex = face.mIndices[element];
				modelData.indices.push_back(vertexIndex);
			}
		}

		// SkiningDataの取得
		for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
			// Assimpに関連づけられたJointからデータを取得
			// ※Joint = Bone
			aiBone* bone = mesh->mBones[boneIndex];
			std::string jointName = bone->mName.C_Str();
			JointWeightData& jointWeightData = modelData.skinClusterData[jointName];

			aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse();
			aiVector3D scale, translate;
			aiQuaternion rotate;
			bindPoseMatrixAssimp.Decompose(scale, rotate, translate);
			Matrix4x4 bindPoseMatrix = MakeAffineMatrix(
				{ scale.x,scale.y,scale.z }, { rotate.x,-rotate.y,-rotate.z,rotate.w }, { -translate.x,translate.y,translate.z });
			jointWeightData.inverseBindPoseMatrix = Matrix4x4::Inverse(bindPoseMatrix);

			// Weight情報を抜き出し
			for (uint32_t weightIndex = 0;weightIndex < bone->mNumWeights;++weightIndex) {
				jointWeightData.vertexWeights.push_back({ bone->mWeights[weightIndex].mWeight, bone->mWeights[weightIndex].mVertexId });
			}
		}
	}

	// ---------------------------
	// Material Data を構築
	// ---------------------------

	for (uint32_t materialIndex = 0;materialIndex < scene->mNumMaterials;++materialIndex) {
		aiMaterial* material = scene->mMaterials[materialIndex];
		if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
			aiString textureFilePath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
			modelData.material.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();
		}
	}

	// ----------------------------
	// Node Data を構築
	// ----------------------------

	modelData.rootNode = ReadNode(scene->mRootNode);

	/////////////////
	// 構築したModelDataをreturnする
	/////////////////
	return modelData;
}

ModelData ModelManager::LoadObjFile(const std::string& filename, const std::string& directoryPath) {
	/////////////////
	// 変数宣言
	/////////////////
	ModelData modelData;
	std::vector<Vector4> positions;
	std::vector<Vector3> normals;
	std::vector<Vector2> texcoords;
	std::string line;

	/////////////////
	// ファイルをひらく
	/////////////////
	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	/////////////////
	// ModelDataを構築する
	/////////////////
	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;
		// 頂点位置
		if (identifier == "v")
		{
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.x *= -1.0f;
			position.w = 1.0f;
			positions.push_back(position);
		}
		// 頂点テクスチャ座標
		else if (identifier == "vt")
		{
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);
		}
		// 頂点法線
		else if (identifier == "vn")
		{
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normal.x *= -1.0f;
			normals.push_back(normal);
		}
		// 面
		else if (identifier == "f")
		{
			// 1行分の頂点定義をすべて取得
			std::vector<std::string> vertexDefs;
			std::string vertexDefinition;
			while (s >> vertexDefinition)
			{
				vertexDefs.push_back(vertexDefinition);
			}

			// 3頂点未満は無視
			if (vertexDefs.size() < 3) continue;

			// 扇形分割で三角形を生成
			for (size_t i = 1; i + 1 < vertexDefs.size(); ++i)
			{
				VertexData triangle[3];
				std::string vdefs[3] = { vertexDefs[0], vertexDefs[i], vertexDefs[i + 1] };
				for (int faceVertex = 0; faceVertex < 3; ++faceVertex)
				{
					std::istringstream v(vdefs[faceVertex]);
					uint32_t elementIndices[3] = {};
					/*for (int element = 0; element < 3; ++element)
					{
						std::string index;
						std::getline(v, index, '/');
						elementIndices[element] = std::stoi(index);
					}*/
					for (int element = 0; element < 3; ++element) {
						std::string index;
						if (!std::getline(v, index, '/')) {
							elementIndices[element] = 0; // デフォルト値 or エラー処理
						}
						else if (index.empty()) {
							elementIndices[element] = 0; // デフォルト値 or エラー処理
						}
						else {
							elementIndices[element] = std::stoi(index);
						}
					}
					Vector4 position = positions[elementIndices[0] - 1];
					//Vector2 texcoord = texcoords[elementIndices[1] - 1];
					Vector2 texcoord;
					if (elementIndices[1] > 0 && elementIndices[1] <= texcoords.size()) {
						texcoord = texcoords[elementIndices[1] - 1];
					}
					else {
						texcoord = { 0.0f, 0.0f }; // デフォルトUV
					}
					Vector3 normal = normals[elementIndices[2] - 1];
					triangle[faceVertex] = { position, texcoord, normal };
				}
				// 頂点の順序を逆にして追加（右手系→左手系変換のため）
				modelData.vertices.push_back(triangle[2]);
				modelData.vertices.push_back(triangle[1]);
				modelData.vertices.push_back(triangle[0]);
			}
		}

		// mtllib
		else if (identifier == "mtllib")
		{
			// materialTemplateLibraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;
			// 基本的にmtlはobjファイルと同一階層に配置指せるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}


	/////////////////
	// 構築したModelDataをreturnする
	/////////////////
	return modelData;
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

MaterialData ModelManager::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
	// 1, 中で必要となる変数の宣言
	MaterialData materialData;// 構築するMaterialData
	std::string line;
	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());
	// 2, ファイルを開く
	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;// 行の最初の文字列を取得
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;// テクスチャファイル名を取得
			// テクスチャファイルのパスを構築
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}


	// 3, 実際にファイルを読み、MaterialDataを構築していく
	// 4, MaterialDataを返す
	return materialData;
}

