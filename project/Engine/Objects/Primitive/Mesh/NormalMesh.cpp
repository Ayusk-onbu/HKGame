#include "NormalMesh.h"
#include "ModelManager.h"
#include "SRVManager.h"
#include "SDFManager.h"
#include "CameraSystem.h"

void NormalMeshModel::Initialize(Fngine* engine, uint32_t numInstance) {
	PrimitiveBaseModel::Initialize(engine, numInstance);

	SetCommandPrePass = [this]() {
		if (instanceDataList_.empty()) return;

		auto commandList = p_engine_->GetCommand().GetList().GetList();

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		auto& prePassPSO = PSOManager::GetInstance()->GetPSO("DepthPrePassNormalMesh");
		commandList->SetGraphicsRootSignature(prePassPSO.GetRootSignature().GetRS().Get());
		commandList->SetPipelineState(prePassPSO.GetGPS().Get());

		// t0: インスタンスバッファのセット
		commandList->SetGraphicsRootDescriptorTable(0, instancingBuffer_->GetSRVHandleGPU());

		// --------------------------------------------------
		// バッチの作成 (MainPass と全く同じロジック)
		// --------------------------------------------------
		struct Batch {
			uint32_t modelID;
			uint32_t startLocation;
			uint32_t count;
		};

		std::vector<Batch> batches;
		batches.reserve(16);

		for (size_t i = 0; i < instanceDataList_.size(); ++i) {
			uint32_t currentID = instanceDataList_[i].modelID;

			if (!batches.empty() && batches.back().modelID == currentID) {
				batches.back().count++;
			}
			else {
				batches.push_back({ currentID, static_cast<uint32_t>(i), 1 });
			}
		}

		// --------------------------------------------------
		// メッシュ切り替え＆PrePass描画発行
		// --------------------------------------------------
		for (const auto& batch : batches) {
			auto& object = ModelManager::GetInstance()->GetObjectData(batch.modelID);

			commandList->IASetVertexBuffers(0, 1, &object.GetVertexBufferView());
			commandList->IASetIndexBuffer(&object.GetIndexBufferView());

			commandList->SetGraphicsRoot32BitConstant(1, batch.startLocation, 0);

			commandList->DrawIndexedInstanced(
				object.GetIndexCount(),
				batch.count,
				0,
				0,
				0
			);
		}
	};

	SetCommand = [this]() {
		if (instanceDataList_.empty()) return;

		auto commandList = p_engine_->GetCommand().GetList().GetList();

		// --------------------------------------------------
		// PSO / ルートシグネチャ / DescriptorTable のセット（全体で1回のみ！）
		// --------------------------------------------------
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->SetGraphicsRootSignature(PSOManager::GetInstance()->GetPSO("NormalMesh").GetRootSignature().GetRS().Get());
		commandList->SetPipelineState(PSOManager::GetInstance()->GetPSO("NormalMesh").GetGPS().Get());

		// Vt0: StructuredBuffer (全モデルの全インスタンスデータが入った1本のバッファ)
		commandList->SetGraphicsRootDescriptorTable(0, instancingBuffer_->GetSRVHandleGPU());
		// Pt0: バインドレステクスチャヒープ
		//commandList->SetGraphicsRootDescriptorTable(1, SRVManager::GetInstance()->GetHeap()->GetGPUDescriptorHandleForHeapStart());
		// Pt1: SDFバッファの情報
		commandList->SetGraphicsRootDescriptorTable(1, SDFManager::GetInstance()->GetInstanceBufferSRVHandleGPU());

		struct ShadowConfig {
			Vector3 lightDir;
			uint32_t activeSDFCount;
		} shadowConfig;

		shadowConfig.lightDir = Normalize(Vector3(0.5f, 1.0f, -0.5f));
		shadowConfig.activeSDFCount = SDFManager::GetInstance()->GetActiveInstanceCount();

		// Pb0: 影やライトの情報
		commandList->SetGraphicsRoot32BitConstants(2, sizeof(ShadowConfig) / sizeof(uint32_t), &shadowConfig, 0);

		// --------------------------------------------------
		// ソート済みデータから Draw Call 単位のバッチを作成
		// --------------------------------------------------
		struct Batch {
			uint32_t modelID;
			uint32_t startLocation;
			uint32_t count;
		};

		std::vector<Batch> batches;
		batches.reserve(16); // ベクター再割り当ての抑制

		for (size_t i = 0; i < instanceDataList_.size(); ++i) {
			uint32_t currentID = instanceDataList_[i].modelID;

			if (!batches.empty() && batches.back().modelID == currentID) {
				batches.back().count++;
			}
			else {
				batches.push_back({ currentID, static_cast<uint32_t>(i), 1 });
			}
		}

		// --------------------------------------------------
		// メッシュ（VB/IB）のみを切り替えて一括描画発行
		// --------------------------------------------------
		for (const auto& batch : batches) {
			// ModelManager から O(1) で該当モデルの ObjectData を取得
			auto& object = ModelManager::GetInstance()->GetObjectData(batch.modelID);

			commandList->IASetVertexBuffers(0, 1, &object.GetVertexBufferView());
			commandList->IASetIndexBuffer(&object.GetIndexBufferView());
			// Vb0: StartLocationだと毎回Indexが０からになり、データがふざけるのでこうした
			commandList->SetGraphicsRoot32BitConstant(3, batch.startLocation, 0);

			commandList->DrawIndexedInstanced(
				object.GetIndexCount(),
				batch.count,
				0,
				0,
				0
			);
		}
		};
}

NormalMeshForGPU NormalMeshModel::ConvertToGPUData(const NormalMeshObjectData& data) {
	NormalMeshForGPU returnData{};
	returnData.WVP = CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(data.worldTransform.mat_);
	returnData.World = data.worldTransform.mat_;
	returnData.color = data.color;
	returnData.textureIndex = data.textureIndex;
	return returnData;
}