#include "MagicPlane.h"
#include "ModelManager.h"
#include "DrawManager.h"

void MagicCircle::Initialize(Fngine* engine, uint32_t numInstance) {
	PrimitiveBaseModel::Initialize(engine, numInstance);

	gpuConfig_ = std::make_unique<ConstantBuffer<MagicCircleConfig>>(engine);
	gpuConfig_->Initialize();

	useModelName_ = "Plane";

	SetCommand = [this]() {
		auto commandList = p_engine_->GetCommand().GetList().GetList();
		auto object = ModelManager::GetInstance()->LoadObjectData(useModelName_);

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandList->SetGraphicsRootSignature(PSOManager::GetInstance()->GetPSO("MagicCircle").GetRootSignature().GetRS().Get());
		commandList->SetPipelineState(PSOManager::GetInstance()->GetPSO("MagicCircle").GetGPS().Get());
		commandList->IASetVertexBuffers(0, 1, &object.GetVertexBufferView());

		commandList->IASetIndexBuffer(&object.GetIndexBufferView());

		commandList->SetGraphicsRootDescriptorTable(0, instancingBuffer_->GetSRVHandleGPU());

		commandList->SetGraphicsRootConstantBufferView(1, gpuConfig_->GetGPUVirtualAddress());

		commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetTexture("magic-circle").GetHandleGPU());

		commandList->SetGraphicsRootDescriptorTable(3, TextureManager::GetInstance()->GetTexture("RevealTex_Circle").GetHandleGPU());

		commandList->SetGraphicsRootDescriptorTable(4, TextureManager::GetInstance()->GetTexture("noise0").GetHandleGPU());

		commandList->DrawIndexedInstanced(object.GetIndexCount(), UINT(instanceDataList_.size()), 0, 0, 0);
		};
}

MagicCircleForGPU MagicCircle::ConvertToGPUData(const MagicCircleData& data) {
	MagicCircleForGPU returnData{};
	returnData.WVP = CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(data.worldTransform.mat_);
	returnData.World = data.worldTransform.mat_;
	returnData.uvTransform = data.uvTransform.mat_;
	returnData.color = data.color;
	return returnData;
}

void MagicCircle::Update() {
	float deltaTime = 1.0f / 60.0f;
	WorldTransform testTransform;
	WorldTransform testUVTransform;
	switch (currentState) {
	case MagicState::Drawing:
		gpuConfig_->GetMappedData()->DrawThreshold += 0.5f * (1.0f / 60.0f); // じわじわ描画
		//gpuConfig_->GetMappedData()->RotationAngle += Deg2Rad(1.0f);
		if (gpuConfig_->GetMappedData()->DrawThreshold >= 1.0f) {
			currentState = MagicState::Active; // 描き終わったら待機状態へ
		}
		break;

	case MagicState::Active:
		activeTimer += deltaTime;

		//gpuConfig_->GetMappedData()->RotationAngle += Deg2Rad(10.0f);
		testTransform.Initialize();
		testTransform.set_.Scale({ 1.5f - activeTimer / 1.35f,0.1f + activeTimer / 0.1f,1.5f - activeTimer / 1.35f });
		testTransform.set_.Rotation({ 0.0f,0.0f, 0.0f });
		testTransform.set_.Translation({ 0.0f,0.01f,0.0f });
		testTransform.LocalToWorld();

		testUVTransform.Initialize();
		testUVTransform.set_.Scale({ 1.0f,1.0f,1.0f });
		testUVTransform.set_.Rotation({ 0.0f,0.0f, 0.0f });
		testUVTransform.set_.Translation({ 0.0f,0.0f,0.0f });
		testUVTransform.LocalToWorld();

		DrawManager::GetInstance()->GetRing()->AddInstance({
			testTransform,
			testUVTransform,
			{1.0f,0.0f,0.0f,1.0f}
		});

		// 例えば2秒間ピカピカさせておく
		if (activeTimer >= 2.0f) {
			currentState = MagicState::Dissolve; // 時間が来たら消滅状態へ
			activeTimer = 0.0f;
		}
		break;

	case MagicState::Dissolve:
		gpuConfig_->GetMappedData()->DissolveThreshold += 1.0f * deltaTime; // シュワァァっと消す

		if (gpuConfig_->GetMappedData()->DissolveThreshold >= 1.0f) {
			activeTimer += deltaTime;
			// 例えば2秒間ピカピカさせておく
			if (activeTimer >= 2.0f) {
				currentState = MagicState::Drawing; // 時間が来たら消滅状態へ
				gpuConfig_->GetMappedData()->DrawThreshold = 0.0f;
				gpuConfig_->GetMappedData()->DissolveThreshold = 0.0f;
				gpuConfig_->GetMappedData()->RotationAngle = 0.0f;
				activeTimer = 0.0f;
			}
		}
		break;
	}

#ifdef USE_IMGUI
	ImGui::Begin("MagicCircle");
	ImGui::DragFloat("DrawThreshold byougaguai", &gpuConfig_->GetMappedData()->DrawThreshold, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("DissolveThreshold syousitu", &gpuConfig_->GetMappedData()->DissolveThreshold, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("rotation kaitenn", &gpuConfig_->GetMappedData()->RotationAngle, 0.01f, 0.0f, 1.0f);
	ImGui::End();
#endif
}