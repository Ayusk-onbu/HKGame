#include "PrimitiveBox.h"
#include "ModelManager.h"

void PrimitiveBox::Initialize(Fngine* engine, uint32_t numInstance) {
	PrimitiveBaseModel::Initialize(engine, numInstance);

	materialResource_ = CreateBufferResource(p_engine_->GetD3D12System().GetDevice().Get(), sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

	// マテリアルの初期化
	materialData_->color = { 1.0f,1.0f,1.0f,1.0f };
	materialData_->enableLighting = false;
	materialData_->uvTransform = Matrix4x4::Make::Identity();
	materialData_->shininess = 0.0f;

	useModelName_ = "cube";

	// =========================
	// 【 PrePass用　コマンド 】
	// =========================
	SetCommandPrePass = [this]() {
		auto commandList = p_engine_->GetCommand().GetList().GetList();
		auto object = ModelManager::GetInstance()->LoadObjectData(useModelName_);

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		auto& prePassPSO = PSOManager::GetInstance()->GetPSO("DepthPrePass");
		commandList->SetGraphicsRootSignature(prePassPSO.GetRootSignature().GetRS().Get());
		commandList->SetPipelineState(prePassPSO.GetGPS().Get());

		// 形状データのセット
		commandList->IASetVertexBuffers(0, 1, &object.GetVertexBufferView());
		commandList->IASetIndexBuffer(&object.GetIndexBufferView());

		commandList->SetGraphicsRootDescriptorTable(0, instancingBuffer_->GetSRVHandleGPU());

		// マテリアルやテクスチャのバインドは省略！
		commandList->DrawIndexedInstanced(object.GetIndexCount(), UINT(instanceDataList_.size()), 0, 0, 0);
	};

	// ==========================
	// 【 MainPass用　コマンド 】
	// ==========================
	SetCommand = [this]() {
		auto commandList = p_engine_->GetCommand().GetList().GetList();
		auto object = ModelManager::GetInstance()->LoadObjectData(useModelName_);

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandList->SetGraphicsRootSignature(PSOManager::GetInstance()->GetPSO("Structured").GetRootSignature().GetRS().Get());
		commandList->SetPipelineState(PSOManager::GetInstance()->GetPSO("Structured").GetGPS().Get());
		commandList->IASetVertexBuffers(0, 1, &object.GetVertexBufferView());
		// IndexBufferView(IBV)の設定
		commandList->IASetIndexBuffer(&object.GetIndexBufferView());
		//マテリアルCBufferの場所を設定
		commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
		//wvp用のCBufferの場所を設定
		commandList->SetGraphicsRootDescriptorTable(1, instancingBuffer_->GetSRVHandleGPU());
		//SRVのDescritorTableの先頭を設定。2はrootParameter[2]である
		commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetTexture("circle").GetHandleGPU());

		commandList->DrawIndexedInstanced(object.GetIndexCount(), UINT(instanceDataList_.size()), 0, 0, 0);
	};
}

PrimitiveBoxForGPU PrimitiveBox::ConvertToGPUData(const PrimitiveBoxData& data) {
	PrimitiveBoxForGPU returnData{};
	returnData.WVP = CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(data.worldTransform.mat_);
	returnData.World = data.worldTransform.mat_;
	returnData.color = data.color;
	return returnData;
}