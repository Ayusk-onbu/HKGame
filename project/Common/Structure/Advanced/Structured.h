#pragma once
#include <wrl.h>
#include"Fngine.h"

template<typename T>
class Structured
{
public:
	Structured(Fngine* fngine) : p_fngine_(fngine) {}
public:
	// 初期化
	void Initialize(uint32_t numElements);

	// CPUからアクセス可能なバッファのポインタ
	T* GetMappedData()const { return mappedData_; }

	// GPU側のSRV Descriptor Handleを取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVHandleGPU()const { return srvAllocation_.gpu; }

    // リソースの取得
	ID3D12Resource* GetResource() const { return resource_.Get(); } // バリア処理用に公開しておく

	// 要素数を取得
	uint32_t GetNumElements()const { return numElements_; }
private:
	Fngine* p_fngine_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource>resource_;
	T* mappedData_ = nullptr;
	uint32_t numElements_ = 0;

    SRVAllocation srvAllocation_;
};

template<typename T>
void Structured<T>::Initialize(uint32_t numElements) {
	numElements_ = numElements;

	// リソースを作成
	resource_ = CreateBufferResource(
		p_fngine_->GetD3D12System().GetDevice().Get(),
		sizeof(T) * numElements_);

	// アドレスを取得
	resource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedData_));

	resource_->SetName(L"StructuredBuffer");

    auto srv = SRVManager::GetInstance();

    SRVAllocation alloc = srv->Allocate();

	// SRVの作成
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	srvDesc.Buffer.NumElements = numElements_;
	srvDesc.Buffer.StructureByteStride = sizeof(T);

	p_fngine_->GetD3D12System().GetDevice()->CreateShaderResourceView(
		resource_.Get(), 
        &srvDesc, 
        alloc.cpu
	);

    this->srvAllocation_ = alloc;

	// 初期化
	std::memset(mappedData_, 0, sizeof(T) * numElements_);
}

template<typename T>
class RWStructured {
public:
    RWStructured(Fngine* fngine) : p_fngine_(fngine) {}

    void Initialize(uint32_t numElements) {
        numElements_ = numElements;
        size_t size = sizeof(T) * numElements_;
        auto device = p_fngine_->GetD3D12System().GetDevice().Get();

		resource_ = CreateBufferResource(device, size, true);

        auto srv = SRVManager::GetInstance();

        SRVAllocation uavAlloc = srv->Allocate();

        // --- UAVの作成 (ComputeShaderで書き込む用) ---
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.NumElements = numElements_;
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		uavDesc.Buffer.StructureByteStride = sizeof(T);

        device->CreateUnorderedAccessView(
            resource_.Get(), nullptr, 
            &uavDesc, uavAlloc.cpu
        );

        this->uavAllocation_ = uavAlloc;

        SRVAllocation srvAlloc = srv->Allocate();

        // --- SRVの作成 (VertexShader等で読み込む用) ---
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.NumElements = numElements_;
        srvDesc.Buffer.StructureByteStride = sizeof(T);

        device->CreateShaderResourceView(
            resource_.Get(), 
            &srvDesc, 
            srvAlloc.cpu
        );

        this->srvAllocation_ = srvAlloc;
    }

    // Compute Shaderにセットする用
    D3D12_GPU_DESCRIPTOR_HANDLE GetUAVHandleGPU() const { return uavAllocation_.gpu; }
    // Vertex/Pixel Shaderにセットする用
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVHandleGPU() const { return srvAllocation_.gpu; }

    ID3D12Resource* GetResource() const { return resource_.Get(); } // バリア処理用に公開しておく

    // 要素数を取得
    uint32_t GetNumElements()const { return numElements_; }
private:
    Fngine* p_fngine_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    uint32_t numElements_ = 0;
	// mappedData_ は無い！CPUからは直接触れない。

    SRVAllocation uavAllocation_;
    SRVAllocation srvAllocation_;
};

class RWTexture2D {
public:
    void Initialize(Fngine* fngine, uint32_t width, uint32_t height, DXGI_FORMAT format) {
        auto device = fngine->GetD3D12System().GetDevice().Get();

        // Defaultヒープで作成
        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        // Texture2D ＋ UAVフラグ
        D3D12_RESOURCE_DESC resDesc{};
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resDesc.Width = width;
        resDesc.Height = height;
        resDesc.DepthOrArraySize = 1;
        resDesc.MipLevels = 1;
        resDesc.Format = format;
        resDesc.SampleDesc.Count = 1;
        resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // これが命！

        device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resource_));

        auto srv = SRVManager::GetInstance();

        SRVAllocation uavAlloc = srv->Allocate();

        // --- UAVの作成 (書き込み用) ---
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.Format = format;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

        device->CreateUnorderedAccessView(
            resource_.Get(), nullptr, 
            &uavDesc, uavAlloc.cpu
        );

        this->uavAllocation_ = uavAlloc;

        SRVAllocation srvAlloc = srv->Allocate();

        // --- SRVの作成 (読み込み用) ---
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = format;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        device->CreateShaderResourceView(
            resource_.Get(), 
            &srvDesc, 
            srvAlloc.cpu
        );

        this->srvAllocation_ = srvAlloc;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GetUAVHandleGPU() const { return uavAllocation_.gpu; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVHandleGPU() const { return srvAllocation_.gpu; }
    ID3D12Resource* GetResource() const { return resource_.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;

    SRVAllocation uavAllocation_;
    SRVAllocation srvAllocation_;
};

class RWTexture3D {
public:
    // depth (奥行き) を引数に追加
    void Initialize(Fngine* fngine, uint32_t width, uint32_t height, uint32_t depth) {
        auto device = fngine->GetD3D12System().GetDevice().Get();

        // Defaultヒープで作成
        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        // Texture3D ＋ UAVフラグ
        D3D12_RESOURCE_DESC resDesc{};
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        resDesc.Width = width;
        resDesc.Height = height;
        resDesc.DepthOrArraySize = static_cast<UINT16>(depth); // depth を設定
        resDesc.MipLevels = 1;
        resDesc.Format = DXGI_FORMAT_R32_FLOAT;
        resDesc.SampleDesc.Count = 1;
        resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resource_));

        auto srv = SRVManager::GetInstance();

        // --- UAVの作成 (書き込み用) ---
        SRVAllocation uavAlloc = srv->Allocate();

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D; // TEXTURE3D に変更
        uavDesc.Texture3D.MipSlice = 0;
        uavDesc.Texture3D.FirstWSlice = 0;
        uavDesc.Texture3D.WSize = depth;                      // 全スライスを指定

        device->CreateUnorderedAccessView(
            resource_.Get(), nullptr,
            &uavDesc, uavAlloc.cpu
        );

        this->uavAllocation_ = uavAlloc;

        // --- SRVの作成 (読み込み用) ---
        SRVAllocation srvAlloc = srv->Allocate();

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D; // ★ TEXTURE3D に変更
        srvDesc.Texture3D.MipLevels = 1;
        srvDesc.Texture3D.MostDetailedMip = 0;

        device->CreateShaderResourceView(
            resource_.Get(),
            &srvDesc,
            srvAlloc.cpu
        );

        this->srvAllocation_ = srvAlloc;
    }

	uint32_t GetSrvIndex() const { return srvAllocation_.index; }

    D3D12_GPU_DESCRIPTOR_HANDLE GetUAVHandleGPU() const { return uavAllocation_.gpu; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVHandleGPU() const { return srvAllocation_.gpu; }
    ID3D12Resource* GetResource() const { return resource_.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    SRVAllocation uavAllocation_;
    SRVAllocation srvAllocation_;
};