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

class ObjectData
{
/////////////////////////
/// 
/// Vertex Resource
///
/////////////////////////
public:
	// Resourceの取得 (ComPtrの参照を返すことで、Mapや直接の操作が可能です)
	Microsoft::WRL::ComPtr<ID3D12Resource>& GetVertexResource() { return vertexResource_; }

	// BufferViewの取得
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return vertexBufferView_; }

	void SetVertexBufferView(const D3D12_VERTEX_BUFFER_VIEW& view) {
		vertexBufferView_ = view;
	}

	// Dataポインタの取得
	VertexData* GetVertexData() const { return vertexData_; }

private:
	Microsoft::WRL::ComPtr <ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
	VertexData* vertexData_;

/////////////////////////
/// 
/// Index Resource
///
/////////////////////////
public:
	// Resourceの取得
	Microsoft::WRL::ComPtr<ID3D12Resource>& GetIndexResource() { return indexResource_; }

	// BufferViewの取得
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const { return indexBufferView_; }

	void SetIndexBufferView(const D3D12_INDEX_BUFFER_VIEW& view) {
		indexBufferView_ = view;
	}

	// Dataポインタの取得
	uint32_t* GetIndexData() const { return indexData_; }

private:
	Microsoft::WRL::ComPtr <ID3D12Resource> indexResource_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_;
	uint32_t* indexData_;
};

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
	Microsoft::WRL::ComPtr <ID3D12Resource> materialResource_;
	Microsoft::WRL::ComPtr <ID3D12Resource> wvpResource_;
public:
	Material* materialData_;
	TransformationMatrix* wvpData_;
	Fngine* fngine_;

	SkinCluster skinCluster_;

	WorldTransform worldTransform_;
	WorldTransform uvTransform_;

	std::string modelName_;
	std::string textureName_;
};

