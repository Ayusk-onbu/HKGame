#pragma once
#include "ISingleton.h"
#include "Structured.h"
#include "../../../UsefulTool/Component/SDF/SDFFunctionSystem.h"

class Fngine;

class SDFFunctionManager : public ISingleton<SDFFunctionManager> {
    friend class ISingleton<SDFFunctionManager>;
public:
    void Initialize(Fngine* fngine);

    // SDFSystem の評価結果を受け取って GPU バッファを更新
    void UpdateGPUBuffer(const std::vector<SDFComposedResult>& results);

    D3D12_GPU_DESCRIPTOR_HANDLE GetInstanceBufferSRVHandleGPU() const {
        return instanceBuffer_ ? instanceBuffer_->GetSRVHandleGPU() : D3D12_GPU_DESCRIPTOR_HANDLE{};
    }
    D3D12_GPU_DESCRIPTOR_HANDLE GetNodeBufferSRVHandleGPU() const {
        return nodeBuffer_ ? nodeBuffer_->GetSRVHandleGPU() : D3D12_GPU_DESCRIPTOR_HANDLE{};
    }

	uint32_t GetActiveInstanceCount()const {
		return activeInstanceCount_;
	}

private:
    void EnsureCapacity(uint32_t instanceCount, uint32_t nodeCount);

private:
    Fngine* pFngine_ = nullptr;
    std::unique_ptr<Structured<SDFFunctionStackInstanceGPU>> instanceBuffer_;
    std::unique_ptr<Structured<SDFFunctionStackNodeGPU>>     nodeBuffer_;
    uint32_t maxInstanceCapacity_ = 0;
    uint32_t maxNodeCapacity_ = 0;
    uint32_t activeInstanceCount_ = 0;
};