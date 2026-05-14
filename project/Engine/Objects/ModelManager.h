#pragma once
#include "ModelObject.h"
#include <unordered_map>


class ModelManager
{
/////////////////////////
/// 
/// 
///
/////////////////////////
public:
	static ModelManager* GetInstance() {
		if (instance_ == nullptr) {
			instance_ = std::make_unique<ModelManager>();
		}
		return instance_.get();
	}
	void ReleaseInstance() { instance_.reset(); }

private:
	static std::unique_ptr<ModelManager>instance_;

/////////////////////////
/// 
/// 
///
/////////////////////////
public:
	void Initialize(Fngine* fngine);

	ObjectData& GetObjectData(const std::string& ID);

	/// <summary>
	/// Modelデータをロードする
	/// </summary>
	/// <param name="filename">ファイルネーム</param>
	/// <param name="directoryPath"></param>
	/// <param name="type">三角面化してないならType::OBJ</param>
	/// <returns></returns>
	std::string LoadObj(const std::string& filename, const std::string& directoryPath = "resources",LoadFileType type = LoadFileType::Assimp);

	void AddObject(const std::string name, const std::vector<VertexData>& vertices, const std::vector<uint32_t>& indices);
private:
	// 図鑑的な存在
	std::unordered_map<std::string, std::unique_ptr<ObjectData>>objects_;

	uint32_t modelCount_;
	Fngine* pFngine_;

private:
	ModelData LoadObjFile(const std::string& filename, const std::string& directoryPath);
	ModelData LoadFiles(const std::string& fileName, const std::string& directoryPath);
	Node ReadNode(aiNode* node);
	MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
};

