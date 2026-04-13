#include "ModelData.h"

void SkinCluster::Create(Fngine* engine, const Skeleton& skeleton, const ModelData& modelData) {
	uint32_t numVertices = static_cast<uint32_t>(modelData.vertices.size());
	uint32_t numJoints = static_cast<uint32_t>(skeleton.joints_.size());
	numVertices_ = numVertices;
	// ==========================================
	// 1. 各バッファのインスタンス化と初期化
	// ==========================================

	palette_ = std::make_unique<Structured<WellForGPU>>(engine);
	palette_->Initialize(numJoints);

	inputVertices_ = std::make_unique<Structured<VertexData>>(engine);
	inputVertices_->Initialize(numVertices);

	influences_ = std::make_unique<Structured<VertexInfluence>>(engine);
	influences_->Initialize(numVertices);

	outputVertices_ = std::make_unique<RWStructured<VertexData>>(engine);
	outputVertices_->Initialize(numVertices);

	skinningInfo_ = std::make_unique<ConstantBuffer<SkinningInformation>>(engine);
	skinningInfo_->Initialize();

	// ==========================================
	// 2. 初期データの書き込み
	// ==========================================

	// b0: 頂点数を定数バッファに書き込む
	skinningInfo_->GetMappedData()->numVertices = numVertices;

	// t1: オリジナルの頂点データをコピーする
	std::copy(modelData.vertices.begin(), modelData.vertices.end(), inputVertices_->GetMappedData());

	// t2: Influenceデータの領域を取得し、ゼロクリアしておく
	VertexInfluence* mappedInfluence = influences_->GetMappedData();
	std::memset(mappedInfluence, 0, sizeof(VertexInfluence) * numVertices);

	// ボーンの初期化（逆バインドポーズ行列の保存領域を作成）
	inverseBindPoseMatrices_.resize(numJoints);
	std::generate(inverseBindPoseMatrices_.begin(), inverseBindPoseMatrices_.end(), []() { return Matrix4x4::Make::Identity(); });

	// ModelDataのSkinCluster情報を解析してInfluenceの中身を埋める
	for (const auto& jointWeight : modelData.skinClusterData) {
		// Jointの名前を取得
		auto it = skeleton.jointMap_.find(jointWeight.first);

		// そんな名前は存在するのかをチェックする
		if (it == skeleton.jointMap_.end()) {
			// 無かったら次の要素へ移行
			continue;
		}

		// 該当のIndexのinverseBindPoseMatrixを取得
		inverseBindPoseMatrices_[(*it).second] = jointWeight.second.inverseBindPoseMatrix;

		for (const auto& vertexWeight : jointWeight.second.vertexWeights) {
			// 該当のInfluenceの情報を取得
			auto& currentInfluence = mappedInfluence[vertexWeight.vertexIndex];
			for (uint32_t index = 0; index < kNumMaxInfluence; ++index) {
				// Weightがあいているかを確認する
				if (currentInfluence.weights[index] == 0.0f) {
					// Weightが0.0fなら代入する
					currentInfluence.weights[index] = vertexWeight.weight;
					currentInfluence.jointIndices[index] = (*it).second;
					break;
				}
			}
		}
	}
}

void SkinCluster::Update(const Skeleton& skeleton) {
	// ラッパークラスから直接マップ済みポインタを取得する
	WellForGPU* mappedPalette = palette_->GetMappedData();

	for (size_t jointIndex = 0; jointIndex < skeleton.joints_.size(); ++jointIndex) {
		assert(jointIndex < inverseBindPoseMatrices_.size());

		mappedPalette[jointIndex].skeletonSpaceMatrix =
			inverseBindPoseMatrices_[jointIndex] * skeleton.joints_[jointIndex].skeletonSpaceMatrix;

		mappedPalette[jointIndex].skeletonSpaceInverseTransMatrix =
			Matrix4x4::Transpose(Matrix4x4::Inverse(mappedPalette[jointIndex].skeletonSpaceMatrix));
	}
}

void SkinCluster::DispatchComputeShader(ID3D12GraphicsCommandList* commandList) {
	D3D12_RESOURCE_BARRIER barrierToUAV = {};
	barrierToUAV.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierToUAV.Transition.pResource = outputVertices_->GetResource();
	barrierToUAV.Transition.StateBefore = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER; // 前フレームで描画に使っていた状態
	barrierToUAV.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS; // 今からCSで書き込む状態

	commandList->ResourceBarrier(1, &barrierToUAV);
	
	// 1. PSOとRootSignatureをセット
	commandList->SetComputeRootSignature(PSOManager::GetInstance()->GetPSO("SkinningCS").GetRootSignature().GetRS().Get());
	commandList->SetPipelineState(PSOManager::GetInstance()->GetPSO("SkinningCS").GetCPS().Get());

	// 2. 各バッファをRootSignatureの番号に合わせてセット
	commandList->SetComputeRootDescriptorTable(0, palette_->GetSRVHandleGPU());
	commandList->SetComputeRootDescriptorTable(1, inputVertices_->GetSRVHandleGPU());
	commandList->SetComputeRootDescriptorTable(2, influences_->GetSRVHandleGPU());

	// 出力先は UAV Handle を渡す！
	commandList->SetComputeRootDescriptorTable(3, outputVertices_->GetUAVHandleGPU());

	// b0は直渡しショートカット！
	commandList->SetComputeRootConstantBufferView(4, skinningInfo_->GetGPUVirtualAddress());

	// 3. Dispatch (スレッドグループ数の計算: 1024で割って切り上げ)
	uint32_t numVertices = skinningInfo_->GetMappedData()->numVertices;
	uint32_t numGroups = (numVertices + 1023) / 1024;
	commandList->Dispatch(numGroups, 1, 1);

	// 4. リソースバリア（UAVから「頂点バッファ」への状態遷移）
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = outputVertices_->GetResource(); // スキニング後のバッファ
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	// ここが重要！
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS; // CSで書き込んでいた
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER; // IAで頂点として使う
	commandList->ResourceBarrier(1, &barrier);
}