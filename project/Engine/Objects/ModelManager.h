#pragma once
#include "ModelObject.h"
#include "BVH.h"
#include "ISingleton.h"
#include <unordered_map>


class ModelManager : public ISingleton<ModelManager>
{
public:
	friend class ISingleton<ModelManager>;

public:
	void Initialize(Fngine* fngine);

	//   ===== ======
	// 【 Model Data 】
	//   ====== =====
public:
	/// <summary>
	/// データを送る
	/// </summary>
	/// <param name="ID"></param>
	/// <returns></returns>
	ModelData& LoadModelData(const std::string& ID);

	/// <summary>
	/// IDに対応したModelDataを取得
	/// </summary>
	/// <param name="id"></param>
	/// <returns></returns>
	ModelData& GetModelData(uint32_t id);

	/// <summary>
	/// BaseDirectoryに含まれるModelのデータをすべてLoadする
	/// </summary>
	/// <param name="baseDirectory"></param>
	void LoadAllModels(const std::string& baseDirectory = "Assets");

	/// <summary>
	/// ファイルからModelデータをロードする
	/// </summary>
	/// <param name="filename">ファイルネーム</param>
	/// <param name="directoryPath"></param>
	/// <param name="type">三角面化してないならType::OBJ</param>
	/// <returns></returns>
	uint32_t LoadObj(const std::string& filename, const std::string& directoryPath = "resources", LoadFileType type = LoadFileType::Assimp);

	//   ======= =====
	// 【 Object Data 】
	//   ====== ======
public:
	/// <summary>
	/// 
	/// </summary>
	/// <param name="ID"></param>
	/// <returns></returns>
	ObjectData& LoadObjectData(const std::string& ID);

	/// <summary>
	/// IDに対応したObjectDataを取得
	/// </summary>
	/// <param name="id"></param>
	/// <returns></returns>
	ObjectData& GetObjectData(uint32_t id);

	/// <summary>
	/// 使用するモデルデータを登録
	/// </summary>
	/// <param name="ID"></param>
	/// <param name="vertices"></param>
	/// <param name="indices"></param>
	void AddObject(const std::string& ID, const std::vector<VertexData>& vertices, const std::vector<uint32_t>& indices);

	/// <summary>
	/// 
	/// </summary>
	/// <param name="filename"></param>
	/// <param name="directoryPath"></param>
	void LoadModelFromFile(const std::string& filename, const std::string& directoryPath = "Assets");
	//   ====== ===
	// 【 BVH Data 】
	//   ====== ===
public:

	/// <summary>
	/// IDに対応したBVHを取得する
	/// </summary>
	/// <param name="ID">Modelの名前</param>
	/// <returns>メッシュを細かくしたデータ</returns>
	const BVH* GetBVH(const std::string& ID) const;

	/// <summary>
	/// IDに対応したBVHを取得する
	/// </summary>
	/// <param name="id"></param>
	/// <returns></returns>
	const BVH* GetBVH(uint32_t id)const;

	// 登録されている BVH モデルの名前一覧を取得（エディタのCombo表示用）
	std::vector<std::string> GetBVHNames() const;

private:
	/// <summary>
	/// 頂点dataやIndexDataから三角形のメッシュを作り出す
	/// </summary>
	/// <returns></returns>
	std::vector<PhysicsTriangle>ExtractPhysicsTriangles(const std::vector<VertexData>& vertices, const std::vector<uint32_t>& indices, const Matrix4x4& mat);

	Node ReadNode(aiNode* node);

	template <typename T>
	void EnsureCapacity(std::vector<T>& vec, uint32_t id) {
		if (id >= vec.size()) {
			vec.resize(id + 1);
		}
	}
private:
	Fngine* pFngine_;
	uint32_t modelCount_;

	// ID（配列インデックス）で管理するデータ群
	std::vector<std::unique_ptr<ModelObject>> models_;
	std::vector<std::unique_ptr<ObjectData>> objects_;
	std::vector<std::unique_ptr<BVH>> bvhs_;
};

