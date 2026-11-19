#include "SRVManager.h"
#include <cassert>

void SRVManager::Initialize(ID3D12Device* device, uint32_t maxCount) {
    maxCount_ = maxCount;
    useIndex_ = 0;

    // 以前のキューがあれば空にしておく
    while (!freeIndices_.empty()) {
        freeIndices_.pop();
    }

    // SRV用ヒープの設定と生成
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = maxCount_;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // 描画で使うので必須
    heapDesc.NodeMask = 0;

    HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap_));
    assert(SUCCEEDED(hr));
	descriptorHeap_->SetName(L"SRV Main Descriptor Heap");

    // デスクリプタ1個分のサイズを取得（環境によってサイズが変わるため必須）
    descriptorSize_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

SRVAllocation SRVManager::Allocate() {
    uint32_t index = 0;

    // ① まず、再利用可能な空きスロットがあるか確認する
    if (!freeIndices_.empty()) {
        index = freeIndices_.front();
        freeIndices_.pop();
    }
    // ② 空きスロットがなければ、新しいインデックスを発行する
    else {
        // ヒープの限界を超えていないかチェック（超えていたらアサートで止める）
        assert(useIndex_ < maxCount_ && "SRV Descriptor Heap is full!");
        index = useIndex_;
        useIndex_++;
    }

    // ③ 決定したインデックスを元に、CPU/GPUハンドルを計算する
    SRVAllocation alloc;
    alloc.index = index;

    alloc.cpu = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    alloc.cpu.ptr += static_cast<SIZE_T>(descriptorSize_ * index);

    alloc.gpu = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
    alloc.gpu.ptr += static_cast<SIZE_T>(descriptorSize_ * index);

    return alloc;
}

SRVAllocation SRVManager::AllocateRange(uint32_t count) {
    assert(useIndex_ + count <= maxCount_ && "SRV Descriptor Heap is full!");

    // フリーリストを使わず、未使用領域の末尾から連続して切り出す
    uint32_t startIndex = useIndex_;
    useIndex_ += count;

    SRVAllocation alloc;
    alloc.index = startIndex;
    alloc.cpu = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    alloc.cpu.ptr += static_cast<SIZE_T>(descriptorSize_ * startIndex);

    alloc.gpu = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
    alloc.gpu.ptr += static_cast<SIZE_T>(descriptorSize_ * startIndex);

    return alloc;
}

void SRVManager::Free(uint32_t index) {
    // 範囲外の不正なインデックスが渡されていないかチェック
    assert(index < maxCount_ && "Invalid SRV index!");

    // 空きリストに追加して、次回のAllocateで再利用できるようにする
    freeIndices_.push(index);
}