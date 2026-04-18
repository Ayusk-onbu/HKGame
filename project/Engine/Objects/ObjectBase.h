#pragma once
#include <wrl.h>
#include "ResourceFunc.h"
#include "Material.h"
#include "TransformationMatrix.h"
#include "VertexData.h"
#include "ModelData.h"
#include "Transform.h"
#include "D3D12System.h"
#include "TheOrderCommand.h"
#include "PipelineStateObject.h"
#include "DirectionLight.h"
#include "TextureManager.h"
#include "WorldTransform.h"

enum class ObjectDrawType {
	SOLID,
	WIREFRAME,
	Animation,
};

class Fngine;

class ObjectBase
{
public:
	void DrawBase(ObjectDrawType type = ObjectDrawType::SOLID);
	void DrawBase(TheOrderCommand& command,PSO& pso,DirectionLight& light,Texture& tex);
	void DrawBase(TheOrderCommand& command, PSO& pso, DirectionLight& light, D3D12_GPU_DESCRIPTOR_HANDLE& tex);
	void DrawIndexBase(ObjectDrawType type = ObjectDrawType::SOLID);
	void DrawIndexBase(TheOrderCommand& command, PSO& pso, DirectionLight& light, Texture& tex);
	void DrawIndexBase(TheOrderCommand& command, PSO& pso, DirectionLight& light, D3D12_GPU_DESCRIPTOR_HANDLE& tex);

	void InitializeMD(Vector4 color, bool isLight);
	void InitializeWVPD();

public:
	void SetColor(const Vector4& color);
	void SetEnableLighting(const bool& isLight) { materialData_->enableLighting = isLight; }
	void SetFngine(Fngine* fngine) { fngine_ = fngine; }

public:
	// Resource
	Microsoft::WRL::ComPtr <ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr <ID3D12Resource> indexResource_;
	Microsoft::WRL::ComPtr <ID3D12Resource> materialResource_;
	Microsoft::WRL::ComPtr <ID3D12Resource> wvpResource_;
public:
	VertexData* vertexData_;
	uint32_t* indexData_;
	Material* materialData_;
	TransformationMatrix* wvpData_;
	Fngine* fngine_;

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_;
	SkinCluster skinCluster_;

	WorldTransform worldTransform_;
	WorldTransform uvTransform_;
	std::string textureName_;
};

