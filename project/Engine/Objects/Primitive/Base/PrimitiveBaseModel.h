#pragma once
#include "Fngine.h"
#include "PrimitiveBaseModelData.h"
#include "Structured.h"

// 処理を一つにするためのクラス
class IDrawable {
public:
	virtual ~IDrawable() = default;
	virtual void DrawInstancedPrePass() = 0;
	virtual void DrawInstanced() = 0;
};

template <typename TForGPU,typename TObjectData>
class PrimitiveBaseModel : public IDrawable
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

	void DrawInstancedPrePass()override {
		if (!SetCommandPrePass || instanceDataList_.empty())return;

		Prepare();

		SetCommandPrePass();
	}

	// 溜まったデータを一気にGPUへ転送し、一括で描画する！
	void DrawInstanced()override {
		if (instanceDataList_.empty()) return;

		Prepare();

		// コマンドリストへの登録
		if (SetCommand) {
			SetCommand();
		}

		// 次のフレームのためにリストをクリア
		instanceDataList_.clear();
		isPrepared_ = false;
	}

	virtual void SortInstances() {}

private:
	// GPU転送とソートの処理(1回 / F)
	void Prepare() {
		// 既に済んでいるならスキップ
		if (isPrepared_)return;

		// ソート
		SortInstances();

		// GPUに転送
		TForGPU* mappedData = instancingBuffer_->GetMappedData();
		for (size_t i = 0; i < instanceDataList_.size(); ++i) {
			// ここで ObjectData から ForGPU への変換を行う
			mappedData[i] = ConvertToGPUData(instanceDataList_[i]);
		}

		// 転送完了
		isPrepared_ = true;
	}

protected:
	// CPUデータからGPUデータへの変換ルール
	virtual TForGPU ConvertToGPUData(const TObjectData& data) = 0;
	
	using CallBack = std::function<void()>;
	CallBack SetCommand = nullptr;
	CallBack SetCommandPrePass = nullptr;

protected:
	Fngine* p_engine_ = nullptr;
	std::unique_ptr<Structured<TForGPU>> instancingBuffer_;
	std::vector<TObjectData> instanceDataList_; // 描画待ちデータのリスト
	uint32_t numMaxInstance_;

	std::string usePSOName_;
	std::string useModelName_;

	bool isPrepared_ = false;// 今フレームでGPU転送したかどうか
};
