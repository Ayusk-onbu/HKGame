#pragma once
#include "Structures.h"
#include "../Base/PrimitiveBaseModel.h"

// GPUに一括送信用 (HLSLのStructuredBufferと一致させる)
struct NormalMeshForGPU {
	Matrix4x4 WVP;
	Matrix4x4 World;
	Vector4 color;
	uint32_t textureIndex;
	float padding[3]; // 16バイトアライメント調整
};

// CPU側で毎フレーム追加するオブジェクトデータ
struct NormalMeshObjectData {
	WorldTransform worldTransform;
	Vector4 color;
	uint32_t textureIndex;
	uint32_t modelID; // StringPool::GetInstance()->GetStringID(...) で得た数値ID
};

class NormalMeshModel : public PrimitiveBaseModel<NormalMeshForGPU, NormalMeshObjectData>
{
public:
	void Initialize(Fngine* engine, uint32_t numInstance) override;

protected:
	// CPU転送前に modelID で高速数値ソート (1クロック比較)
	void SortInstances() override {
		std::sort(instanceDataList_.begin(), instanceDataList_.end(),
			[](const NormalMeshObjectData& a, const NormalMeshObjectData& b) {
				return a.modelID < b.modelID;
		});
	}

	// CPU用データからGPU用データへの変換ルール
	NormalMeshForGPU ConvertToGPUData(const NormalMeshObjectData& data) override;
};