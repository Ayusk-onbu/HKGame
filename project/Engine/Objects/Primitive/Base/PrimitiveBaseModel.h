#pragma once
#include "Fngine.h"
#include "PrimitiveBaseModelData.h"
#include "Structured.h"

template <typename TForGPU,typename TObjectData>
class PrimitiveBaseModel
{
public:
	virtual ~PrimitiveBaseModel() = default;
	virtual void Initialize(Fngine* engine, uint32_t numInstance) {
		p_engine_ = engine;
		numMaxInstance_ = numInstance;

		instancingBuffer_ = std::make_unique<Structured<TForGPU>>(p_engine_);
		instancingBuffer_->Initialize(numMaxInstance_);

		// CPU側の溜め込み用配列を確保
		instanceDataList_.reserve(numMaxInstance_);
	}

	// 毎フレーム、描画したいオブジェクトのデータを追加する（Drawは呼ばない）
	void AddInstance(const TObjectData& data) {
		if (instanceDataList_.size() < numMaxInstance_) {
			instanceDataList_.push_back(data);
		}
	}

	// 溜まったデータを一気にGPUへ転送し、一括で描画する！
	void DrawInstanced() {
		if (instanceDataList_.empty()) return;

		// 1. CPU -> GPUへのデータ転送（Map）
		TForGPU* mappedData = instancingBuffer_;
		for (size_t i = 0; i < instanceDataList_.size(); ++i) {
			// ここで ObjectData から ForGPU への変換を行う
			// （例：Transformから行列を作ってForGPUに詰めるなど）
			mappedData[i] = ConvertToGPUData(instanceDataList_[i]);
		}
		instancingBuffer_->Unmap();

		// 2. コマンドリストへの登録
		auto commandList = p_engine_->GetCommand().GetList().GetList();

		if (usePSOName_ == "") {

		}

		// 次のフレームのためにリストをクリア
		instanceDataList_.clear();
	}
protected:
	// CPUデータからGPUデータへの変換ルール
	virtual TForGPU ConvertToGPUData(const TObjectData& data) = 0;

private:
	Fngine* p_engine_ = nullptr;
	std::unique_ptr<Structured<TForGPU>> instancingBuffer_;
	std::vector<TObjectData> instanceDataList_; // 描画待ちデータのリスト
	uint32_t numMaxInstance_;

	std::string usePSOName_;
	std::string useModelName_;
};
