#pragma once
#include <wrl.h>
#include"Fngine.h"

template<typename T>
class ConstantBuffer {
public:
    ConstantBuffer(Fngine* fngine) : p_fngine_(fngine) {}

    void Initialize() {
        // 定数バッファは256バイトの倍数にする必要がある（重要！）
        size_t size = (sizeof(T) + 255) & ~255;

        // Uploadヒープで作成 (ResourceFunc.h の関数を想定)
        resource_ = CreateBufferResource(p_fngine_->GetD3D12System().GetDevice().Get(), size);

        // 常にMapしっぱなしにする
        resource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedData_));
    }

    T* GetMappedData() const { return mappedData_; }

    // Root Signatureの SetGraphicsRootConstantBufferView に直接渡す用！
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const {
        return resource_->GetGPUVirtualAddress();
    }

private:
    Fngine* p_fngine_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    T* mappedData_ = nullptr;
};
