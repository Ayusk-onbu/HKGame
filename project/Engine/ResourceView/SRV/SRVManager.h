#pragma once
#include "ShaderResourceView.h"
#include "ISingleton.h"
#include <queue>

class SRVManager :
	public ISingleton<SRVManager>
{
public:
	friend class ISingleton<SRVManager>;
    // 初期化（ヒープの作成とフリーリストの準備）
    void Initialize(ID3D12Device* device, uint32_t maxCount = 1024);

    // 新しいSRVスロットを割り当てる（CPU/GPUハンドルとインデックスを返す）
    SRVAllocation Allocate();
    // 連続した count 個の先頭を返す
    SRVAllocation AllocateRange(uint32_t count);

    // 使わなくなったSRVスロットを返却し、再利用可能にする
    void Free(uint32_t index);

    // 描画時にコマンドリストにセットするためのヒープを取得
    ID3D12DescriptorHeap* GetHeap() const { return descriptorHeap_.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
    uint32_t descriptorSize_ = 0;
    uint32_t maxCount_ = 0;

    // 現在までに新しく発行したインデックスの最大値
    uint32_t useIndex_ = 0;

    // Free()によって返却され、再利用を待っている空きインデックスのリスト
    std::queue<uint32_t> freeIndices_;
};