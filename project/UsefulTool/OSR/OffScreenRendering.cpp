#include "OffScreenRendering.h"
#include<dxgi1_6.h>
#include <d3d12.h>
#include <cassert>
#include "ResourceBarrier.h"
#include "SRVManager.h"


void RenderTargetGroup::Initialize(
    D3D12System& d3d12,
    float width,
    float height,
    const std::vector<RenderTargetSlotDesc>& slots,
    bool isDoubleBuffer,
    bool useDSV
)
{
    assert(!slots.empty() && "RenderTargetGroup: slots must not be empty.");

    width_ = static_cast<uint32_t>(width);
    height_ = static_cast<uint32_t>(height);
    slotDescs_ = slots;
    isDoubleBuffer_ = isDoubleBuffer;
    useDSV_ = useDSV;
    currentBufferIndex_ = 0;

    // ビューポートとシザーの設定
    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.Width = width;
    viewport_.Height = height;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = static_cast<LONG>(width_);
    scissorRect_.bottom = static_cast<LONG>(height_);

    size_t slotCount = slotDescs_.size();
    size_t bufferCount = isDoubleBuffer_ ? 2 : 1;
    UINT totalRTVCount = static_cast<UINT>(slotCount * bufferCount);

    // 1. RTVヒープの作成（連続領域を一括確保）
    rtvHeap_.CreateDescriptorHeap(
        d3d12.GetDevice().Get(),
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        totalRTVCount,
        false
    );

    UINT handleIncrementSize = d3d12.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE startHandle = rtvHeap_.descriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    // 2. リソース、RTV、SRVの確保
    for (size_t b = 0; b < bufferCount; ++b) {
        renderTargets_[b].resize(slotCount);
        rtvHandles_[b].resize(slotCount);
        srvAllocations_[b].resize(slotCount);

        for (size_t i = 0; i < slotCount; ++i) {
            const auto& desc = slotDescs_[i];

            // --- A. テクスチャリソースの作成 ---
            D3D12_RESOURCE_DESC textureDesc{};
            textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            textureDesc.Width = width_;
            textureDesc.Height = height_;
            textureDesc.DepthOrArraySize = 1;
            textureDesc.MipLevels = 1;
            textureDesc.Format = desc.format;
            textureDesc.SampleDesc.Count = 1;
            textureDesc.SampleDesc.Quality = 0;
            textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

            D3D12_CLEAR_VALUE clearValue{};
            clearValue.Format = desc.format;
            clearValue.Color[0] = desc.clearColor[0];
            clearValue.Color[1] = desc.clearColor[1];
            clearValue.Color[2] = desc.clearColor[2];
            clearValue.Color[3] = desc.clearColor[3];

            D3D12_HEAP_PROPERTIES heapProps{};
            heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

            HRESULT hr = d3d12.GetDevice()->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &textureDesc,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                &clearValue,
                IID_PPV_ARGS(&renderTargets_[b][i])
            );
            assert(SUCCEEDED(hr));

#if defined(_DEBUG)
            wchar_t debugName[64];
            swprintf_s(debugName, L"RTGroup_Tex_Buf%d_Slot%d", static_cast<int>(b), static_cast<int>(i));
            renderTargets_[b][i]->SetName(debugName);
#endif

            // --- B. RTVの作成 ---
            size_t globalRTVIndex = b * slotCount + i;
            rtvHandles_[b][i].ptr = startHandle.ptr + (globalRTVIndex * handleIncrementSize);

            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
            rtvDesc.Format = desc.format;
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice = 0;
            rtvDesc.Texture2D.PlaneSlice = 0;

            d3d12.GetDevice()->CreateRenderTargetView(renderTargets_[b][i].Get(), &rtvDesc, rtvHandles_[b][i]);

            // --- C. SRVの作成 ---
            srvAllocations_[b][i] = SRVManager::GetInstance()->Allocate();

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = desc.format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = 1;
            srvDesc.Texture2D.PlaneSlice = 0;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

            d3d12.GetDevice()->CreateShaderResourceView(
                renderTargets_[b][i].Get(),
                &srvDesc,
                srvAllocations_[b][i].cpu
            );
        }
    }

    if (useDSV_) {
        // 3. 深度バッファ初期化
        dsv_.InitializeHeap(d3d12);
        dsv_.MakeResource(d3d12, static_cast<int32_t>(width_), static_cast<int32_t>(height_));
    }
}

void RenderTargetGroup::Initialize(
    D3D12System& d3d12,
    float width,
    float height,
    const std::vector<DXGI_FORMAT>& formats,
    bool isDoubleBuffer,
    bool useDSV
)
{
    std::vector<RenderTargetSlotDesc> slots;
    slots.reserve(formats.size());
    for (const auto& fmt : formats) {
        slots.emplace_back(fmt, true); // デフォルトでクリア有効
    }
    Initialize(d3d12, width, height, slots, isDoubleBuffer, useDSV);
}

void RenderTargetGroup::Begin(TheOrderCommand& command, bool clearDepth) {
    auto commandList = command.GetList().GetList().Get();
    size_t slotCount = slotDescs_.size();

    // 書き込み対象バッファのバリア変更 (PIXEL_SHADER_RESOURCE -> RENDER_TARGET)
    std::vector<D3D12_RESOURCE_BARRIER> barriers(slotCount);
    for (size_t i = 0; i < slotCount; ++i) {
        barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[i].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barriers[i].Transition.pResource = renderTargets_[currentBufferIndex_][i].Get();
        barriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[i].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

    // OMSetRenderTargets（連続したRTV配列をそのまま渡す）
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle{};
	D3D12_CPU_DESCRIPTOR_HANDLE* pDsvHandle = nullptr;
    
    if (useDSV_) {
        dsvHandle = dsv_.GetCPUHandle(DSV_HANDLE_TYPE::Normal);
        pDsvHandle = &dsvHandle;
    }

    commandList->OMSetRenderTargets(
        static_cast<UINT>(slotCount),
        rtvHandles_[currentBufferIndex_].data(),
        FALSE,
        pDsvHandle
    );

    // ビューポートとシザー
    commandList->RSSetViewports(1, &viewport_);
    commandList->RSSetScissorRects(1, &scissorRect_);

    // クリア処理
    Clear(command, useDSV_ && clearDepth);
}

void RenderTargetGroup::End(TheOrderCommand& command, bool autoSwap) {
    auto commandList = command.GetList().GetList().Get();
    size_t slotCount = slotDescs_.size();

    // バリアを元に戻す (RENDER_TARGET -> PIXEL_SHADER_RESOURCE)
    std::vector<D3D12_RESOURCE_BARRIER> barriers(slotCount);
    for (size_t i = 0; i < slotCount; ++i) {
        barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[i].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barriers[i].Transition.pResource = renderTargets_[currentBufferIndex_][i].Get();
        barriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[i].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
    commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

    if (autoSwap) {
        SwapBuffers();
    }
}

void RenderTargetGroup::SwapBuffers() {
    if (isDoubleBuffer_) {
        currentBufferIndex_ = 1 - currentBufferIndex_;
    }
}

void RenderTargetGroup::Clear(TheOrderCommand& command, bool clearDepth) {
    auto commandList = command.GetList().GetList().Get();

    // 各スロットごとの doClear フラグを見て個別クリア
    for (size_t i = 0; i < slotDescs_.size(); ++i) {
        if (slotDescs_[i].doClear) {
            ClearSlot(command, i);
        }
    }

    if (clearDepth) {
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsv_.GetCPUHandle(DSV_HANDLE_TYPE::Normal);
        commandList->ClearDepthStencilView(
            dsvHandle,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            1.0f,
            0,
            0,
            nullptr
        );
    }
}

void RenderTargetGroup::ClearSlot(TheOrderCommand& command, size_t index) {
    assert(index < slotDescs_.size() && "RenderTargetGroup: Index out of range.");
    command.GetList().GetList().Get()->ClearRenderTargetView(
        rtvHandles_[currentBufferIndex_][index],
        slotDescs_[index].clearColor,
        0,
        nullptr
    );
}

void RenderTargetGroup::SetSlotClearEnabled(size_t index, bool enable) {
    assert(index < slotDescs_.size() && "RenderTargetGroup: Index out of range.");
    slotDescs_[index].doClear = enable;
}

void RenderTargetGroup::SetSlotClearColor(size_t index, const float color[4]) {
    assert(index < slotDescs_.size() && "RenderTargetGroup: Index out of range.");
    for (int i = 0; i < 4; ++i) {
        slotDescs_[index].clearColor[i] = color[i];
    }
}

// --- アクセサ群 ---

Microsoft::WRL::ComPtr<ID3D12Resource> RenderTargetGroup::GetResource(size_t index) const {
    assert(index < slotDescs_.size() && "RenderTargetGroup: Index out of range.");
    return renderTargets_[currentBufferIndex_][index];
}

D3D12_GPU_DESCRIPTOR_HANDLE RenderTargetGroup::GetSRVGPUHandle(size_t index) const {
    assert(index < slotDescs_.size() && "RenderTargetGroup: Index out of range.");
    return srvAllocations_[currentBufferIndex_][index].gpu;
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetGroup::GetRTVCPUHandle(size_t index) const {
    assert(index < slotDescs_.size() && "RenderTargetGroup: Index out of range.");
    return rtvHandles_[currentBufferIndex_][index];
}

Microsoft::WRL::ComPtr<ID3D12Resource> RenderTargetGroup::GetPrevResource(size_t index) const {
    assert(index < slotDescs_.size() && "RenderTargetGroup: Index out of range.");
    uint32_t prevIndex = isDoubleBuffer_ ? (1 - currentBufferIndex_) : currentBufferIndex_;
    return renderTargets_[prevIndex][index];
}

D3D12_GPU_DESCRIPTOR_HANDLE RenderTargetGroup::GetPrevSRVGPUHandle(size_t index) const {
    assert(index < slotDescs_.size() && "RenderTargetGroup: Index out of range.");
    uint32_t prevIndex = isDoubleBuffer_ ? (1 - currentBufferIndex_) : currentBufferIndex_;
    return srvAllocations_[prevIndex][index].gpu;
}

void RenderTargetGroup::ChangeDSVHandleType(TheOrderCommand& command, DSV_HANDLE_TYPE type) {
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsv_.GetCPUHandle(type);
    // OFF用のやつをチェンジかな
    command.GetList().GetList()->OMSetRenderTargets(
        static_cast<UINT>(slotDescs_.size()),  // スロット数分（MRTにも対応）
        rtvHandles_[currentBufferIndex_].data(), // 連続したRTVハンドルの先頭ポインタ
        FALSE,
        &dsvHandle
    );
}

//void OffScreenRendering::Initialize(D3D12System& d3d12, float width, float height,
//	DXGI_FORMAT fmt,D3D12_RTV_DIMENSION dimension) {
//	
//
//	D3D12_RESOURCE_DESC renderingDesc{};
//	//   テクスチャの種類
//	renderingDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
//	//   解像度に関する
//	// 幅
//	renderingDesc.Width = UINT(width);
//	// 高さ
//	renderingDesc.Height = UINT(height);
//	//   Z　or　配列のサイズ
//	renderingDesc.DepthOrArraySize = 1;
//	//   何個同じリソースを作るか
//	// ミップマップレベルの数数に応じて解像度が小さいリソースが用意される
//	// Renderingするので１でいい
//	renderingDesc.MipLevels = 1;
//	//   ピクセルのフォーマット
//	renderingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
//	//   サンプル数
//	renderingDesc.SampleDesc.Count = 1;
//	//   リソースのフラグ
//	renderingDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
//
//	// 関数化したほうが使いやすそう
//	D3D12_CLEAR_VALUE clearValue{};
//	clearValue.Format = renderingDesc.Format;
//	clearValue.Color[0] = 0.0f;
//	clearValue.Color[1] = 0.0f;
//	clearValue.Color[2] = 0.0f;
//	clearValue.Color[3] = 0.0f;
//
//	//   利用するHeapの設定
//	D3D12_HEAP_PROPERTIES heapProperties = {};
//	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
//	HRESULT hr = d3d12.GetDevice()->CreateCommittedResource(
//		&heapProperties,
//		D3D12_HEAP_FLAG_NONE,
//		&renderingDesc,
//		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
//		&clearValue,
//		IID_PPV_ARGS(&offScreenTexture_));
//	assert(SUCCEEDED(hr));
//
//	offRTV_.Initialize(&d3d12, offScreenTexture_, fmt, dimension);
//
//	offScreenTexture_->SetName(L"オフスクリーン");
//
//	//////////////////// ここまでがRTVの設定 ///////////////////////////
//
//	SRVAllocation alloc = SRVManager::GetInstance()->Allocate();
//
//	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
//	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
//	//ZeroMemory(&srvDesc_, sizeof(srvDesc_));
//	srvDesc.Format = offScreenTexture_->GetDesc().Format;
//	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
//	srvDesc.Texture2D.MostDetailedMip = 0;
//	srvDesc.Texture2D.MipLevels = renderingDesc.MipLevels;
//	
//	d3d12.GetDevice()->CreateShaderResourceView(
//		offScreenTexture_.Get(), 
//		&srvDesc, 
//		alloc.cpu
//	);
//
//	this->srvAllocation_ = alloc;
//
//	dsv_.InitializeHeap(d3d12);
//	dsv_.MakeResource(d3d12,int32_t(width),int32_t(height)); 
//}
//
//void OffScreenRendering::Begin(TheOrderCommand& command) {
//	
//	// レンダーターゲットをクリアする色 (例: 青)
//	FLOAT clearColor[] = { 0.0f,0.0f,0.0f,0.0f }; // R, G, B, A
//
//	// コマンドリストにレンダーターゲットを設定
//	// 深度ステンシルバッファを使用する場合は、DSVハンドルもここに渡す
//	//command.GetList().GetList()->OMSetRenderTargets(1, &offRTV_.GetHandle(), FALSE, nullptr); // FALSE は単一 RTV の場合
//	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsv_.GetCPUHandle(DSV_HANDLE_TYPE::Normal);
//	command.GetList().GetList()->OMSetRenderTargets(1, &offRTV_.GetHandle(), false, &dsvHandle);
//
//	command.GetList().GetList()->ClearRenderTargetView(offRTV_.GetHandle(), clearColor, 0, nullptr);
//	command.GetList().GetList()->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);//
//}
//
//void OffScreenRendering::End(TheOrderCommand& command) {
//	//ResourceBarrier barrier = {};
//	//barrier.SetTransition(command.GetList().GetList().Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
//}
//
//void OffScreenRendering::ChangeDSVHandleType(TheOrderCommand& command,DSV_HANDLE_TYPE type) {
//	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsv_.GetCPUHandle(type);
//	command.GetList().GetList()->OMSetRenderTargets(1, &offRTV_.GetHandle(), false, &dsvHandle);
//}