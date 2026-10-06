#pragma once
#include "Structures.h"
#include "VertexData.h"
#include "../Base/PrimitiveBaseModel.h"

#include <vector>
// リング作成用data
struct RingData {
	uint32_t ringDivide = 32;// 分割数
	float outerRadius = 1.0f;// 外半径
	float innerRadius = 0.2f;// 内半径
};
// GPU用
struct PrimitiveRingForGPU {
	Matrix4x4 WVP;
	Matrix4x4 World;
	Matrix4x4 uvTransform;
	Vector4 color;
};
// CPU用
struct PrimitiveRingData {
	WorldTransform worldTransform;
	WorldTransform uvTransform;
	Vector4 color;
};

std::vector<VertexData> MakeObjectVertices(const RingData& data);

std::vector<uint32_t> MakeObjectIndices(const RingData& data);

class PrimitiveRing :
	public PrimitiveBaseModel<PrimitiveRingForGPU, PrimitiveRingData>
{
public:
	void Initialize(Fngine* engine, uint32_t numInstance)override;
private:
	PrimitiveRingForGPU ConvertToGPUData(const PrimitiveRingData& data)override;

};