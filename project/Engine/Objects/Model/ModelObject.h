#pragma once
#include "ObjectBase.h"
#include "Structured.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

enum class LoadFileType {
	Assimp,
	OBJ,
};

class ModelObject
	:public ObjectBase
{
public:
	~ModelObject();
public:
	void Initialize(Fngine* fngine);
	void Initialize(D3D12System& d3d12, const std::string& filename, const std::string& directoryPath = "resources", LoadFileType type = LoadFileType::Assimp);

	void Draw(ObjectDrawType type = ObjectDrawType::SOLID);
	void Draw(TheOrderCommand& command, PSO& pso, DirectionLight& light, Texture& tex);
	void Draw(TheOrderCommand& command, PSO& pso, DirectionLight& light, D3D12_GPU_DESCRIPTOR_HANDLE& tex);

	ModelData LoadObjFile(const std::string& filename, const std::string& directoryPath);
	ModelData LoadFiles(const std::string& fileName, const std::string& directoryPath);
	void LocalToWorld();

	//--=---=--- - + - ---=---=--
	// Set Functions
	//--=---=--- - + - ---=---=--

	void SetWVPData(Matrix4x4 WVP);
	void SetModelData(ModelData& modelData) { modelData_ = modelData; }

	//========== - + - ==========
	// Get Functions
	//========== - + - ==========

	std::string& GetFilePath() { return modelData_.material.textureFilePath; }
	// ModelDataの取得と参照
	ModelData& GetModelData() { return modelData_; }
	// [ Node ]
	Node& GetNode() { return modelData_.rootNode; }
private:

	//========== - + - ==========
	// Private Functions
	//========== - + - ==========

	void InitializeResource(D3D12System& d3d12,const std::string& filename, const std::string& directoryPath, LoadFileType type);
	void InitializeResource(D3D12System& d3d12, ModelData& modelData);
	void InitializeData();
	Node ReadNode(aiNode* node);
	MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
private:
	ModelData modelData_;
public:
	std::string modelName_;
};