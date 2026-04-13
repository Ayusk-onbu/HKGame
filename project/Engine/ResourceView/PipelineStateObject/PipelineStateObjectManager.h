#pragma once
#include "PipelineStateObject.h"
#include <unordered_map>

class PipelineStateObjectManager
{
public:
	static PipelineStateObjectManager* GetInstance() {
		if (instance_ == nullptr) {
			instance_ = std::make_unique<PipelineStateObjectManager>();
		}
		return instance_.get();
	}
	static void ReleaseInstance() { instance_.reset(); }
public:
	void Initialize(Fngine* fngine);

	/// <summary>
	/// PSOを作成する関数
	/// </summary>
	/// <param name="key　セッティングの内容"></param>
	/// <param name="name　キー"></param>
	void CreateNewPSO(const PSOKey& key, const std::string& name);

	/// <summary>
	/// 
	/// </summary>
	/// <param name="name"></param>
	PSO& GetPSO(const std::string& name);

	/// <summary>
	/// 主にBlendModeのImGuiである
	/// </summary>
	void ImGui();
private:
	static std::unique_ptr<PipelineStateObjectManager>instance_;

	// これここなのかな？
	//void CreateGraphicsPipelineState();
	//void CreateComputePipelineState();

	Fngine* p_fngine_ = nullptr;
	std::unordered_map<std::string, PSO>PSOs_;
};

using PSOManager = PipelineStateObjectManager;

