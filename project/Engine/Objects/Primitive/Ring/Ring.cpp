#include "Ring.h"
#include "ModelManager.h"

std::vector<VertexData> MakeObjectVertices(const RingData& data) {
	std::vector<VertexData>vertices;
	vertices.reserve(data.ringDivide * 6);// 1分割あたり6頂点 (三角形2つ分)
	
	for (uint32_t index = 0; index < data.ringDivide; ++index) {
		float radianPerDivide = 2.0f * 3.14159265f / static_cast<float>(data.ringDivide);
		float angle = radianPerDivide * index;
		float sin = std::sinf(angle);
		float cos = std::cosf(angle);

		float nextAngle = radianPerDivide * (index + 1);
		float sinNext = std::sinf(nextAngle);
		float cosNext = std::cosf(nextAngle);

		float u = static_cast<float>(index) / static_cast<float>(data.ringDivide);
		float uNext = static_cast<float>(index + 1) / static_cast<float>(data.ringDivide);

		VertexData v1{
			{-sin * data.outerRadius, cos * data.outerRadius, 0.0f, 1.0f},
			{u, 0.0f},
			{0.0f, 0.0f, -1.0f }
		};
		VertexData v2{
			{-sinNext * data.outerRadius, cosNext * data.outerRadius, 0.0f, 1.0f},
			{uNext, 0.0f},
			{0.0f, 0.0f, -1.0f },
		};
		VertexData v3{
			{-sin * data.innerRadius, cos * data.innerRadius, 0.0f, 1.0f},
			{u, 1.0f},
			{0.0f, 0.0f, -1.0f },
		};
		VertexData v4{
			{-sinNext * data.innerRadius, cosNext * data.innerRadius, 0.0f, 1.0f},
			{uNext, 1.0f},
			{0.0f, 0.0f, -1.0f },
		};

		vertices.push_back(v1);
		vertices.push_back(v2);
		vertices.push_back(v3);
		vertices.push_back(v4);
	}

	return vertices;
}

std::vector<uint32_t> MakeObjectIndices(const RingData& data) {
	std::vector<uint32_t>indices;
	indices.reserve(data.ringDivide * 6);// 1分割あたり6頂点 (三角形2つ分)
	for (uint32_t index = 0; index < data.ringDivide; ++index) {
		uint32_t baseIndex = index * 4;
		// 三角形1
		indices.push_back(baseIndex + 0);
		indices.push_back(baseIndex + 1);
		indices.push_back(baseIndex + 2);
		// 三角形2
		indices.push_back(baseIndex + 2);
		indices.push_back(baseIndex + 1);
		indices.push_back(baseIndex + 3);
	}
	return indices;
}

void PrimitiveRing::Initialize(Fngine* engine, uint32_t numInstance) {
	PrimitiveBaseModel::Initialize(engine, numInstance);

	useModelName_ = "Ring";

	// ==========================
	// 【 MainPass用　コマンド 】
	// ==========================
	SetCommand = [this]() {
		auto commandList = p_engine_->GetCommand().GetList().GetList();
		auto object = ModelManager::GetInstance()->LoadObjectData(useModelName_);

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandList->SetGraphicsRootSignature(PSOManager::GetInstance()->GetPSO("Ring").GetRootSignature().GetRS().Get());
		commandList->SetPipelineState(PSOManager::GetInstance()->GetPSO("Ring").GetGPS().Get());
		commandList->IASetVertexBuffers(0, 1, &object.GetVertexBufferView());
		
		commandList->IASetIndexBuffer(&object.GetIndexBufferView());
		
		commandList->SetGraphicsRootDescriptorTable(0, instancingBuffer_->GetSRVHandleGPU());
	
		commandList->SetGraphicsRootDescriptorTable(1, TextureManager::GetInstance()->GetTexture("GridLine").GetHandleGPU());

		commandList->DrawIndexedInstanced(object.GetIndexCount(), UINT(instanceDataList_.size()), 0, 0, 0);
		};
}

PrimitiveRingForGPU PrimitiveRing::ConvertToGPUData(const PrimitiveRingData& data) {
	PrimitiveRingForGPU returnData{};
	returnData.WVP = CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(data.worldTransform.mat_);
	returnData.World = data.worldTransform.mat_;
	returnData.uvTransform = data.uvTransform.mat_;
	returnData.color = data.color;
	return returnData;
}