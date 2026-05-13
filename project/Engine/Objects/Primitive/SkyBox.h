#pragma once
#include "ObjectBase.h"

struct SkyBoxMaterial {
	Vector4 color;
};

struct SkyBoxTransformationMatrix {
	Matrix4x4 WVP;
};

class SkyBox {
public:
	void Initialize(Fngine* engine, std::string textureName = "GridLine");
	void Draw();
private:
	Fngine* p_engine_;
	WorldTransform worldTransform_;

	//   ========
	// 【 Vertex 】
	//   ========
	Microsoft::WRL::ComPtr <ID3D12Resource> vertexResource_;
	SkyBoxVertexData* vertexData_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;

	//   =======
	// 【 Index 】
	//   =======
	Microsoft::WRL::ComPtr <ID3D12Resource> indexResource_;
	uint32_t* indexData_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_;

	//   =====
	// 【 WVP 】
	//   =====
	Microsoft::WRL::ComPtr <ID3D12Resource> wvpResource_;
	SkyBoxTransformationMatrix* wvpData_;

	//   ==========
	// 【 Material 】
	//   ==========
public:
	void SetColor(const Vector3& color) { materialData_->color = { color.x,color.y,color.z,materialData_->color.w };}

private:
	Microsoft::WRL::ComPtr <ID3D12Resource> materialResource_;
	SkyBoxMaterial* materialData_;

	std::string textureName_;
};