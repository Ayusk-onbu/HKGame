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
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVHandleGPU()const { return srvHandleGPU_; }

	// 要素数を取得
	uint32_t GetNumElements()const { return numElements_; }
private:
	Fngine* p_fngine_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource>resource_;
	T* mappedData_ = nullptr;
	uint32_t numElements_ = 0;

	// SRV設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc_{};
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU_{};
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_{};
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

	// SRVの作成
	srvDesc_.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc_.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc_.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc_.Buffer.FirstElement = 0;
	srvDesc_.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	srvDesc_.Buffer.NumElements = numElements_;
	srvDesc_.Buffer.StructureByteStride = sizeof(T);

	srvHandleCPU_ = p_fngine_->GetSRV().GetCPUDescriptorHandle();
	srvHandleGPU_ = p_fngine_->GetSRV().GetGPUDescriptorHandle();

	p_fngine_->GetD3D12System().GetDevice()->CreateShaderResourceView(
		resource_.Get(), &srvDesc_, srvHandleCPU_
	);

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

        // --- UAVの作成 (ComputeShaderで書き込む用) ---
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.NumElements = numElements_;
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		uavDesc.Buffer.StructureByteStride = sizeof(T);

        uavHandleCPU_ = p_fngine_->GetSRV().GetCPUDescriptorHandle();
        uavHandleGPU_ = p_fngine_->GetSRV().GetGPUDescriptorHandle();
        device->CreateUnorderedAccessView(resource_.Get(), nullptr, &uavDesc, uavHandleCPU_);

        // --- SRVの作成 (VertexShader等で読み込む用) ---
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.NumElements = numElements_;
        srvDesc.Buffer.StructureByteStride = sizeof(T);

        srvHandleCPU_ = p_fngine_->GetSRV().GetCPUDescriptorHandle();
        srvHandleGPU_ = p_fngine_->GetSRV().GetGPUDescriptorHandle();
        device->CreateShaderResourceView(resource_.Get(), &srvDesc, srvHandleCPU_);
    }

    // Compute Shaderにセットする用
    D3D12_GPU_DESCRIPTOR_HANDLE GetUAVHandleGPU() const { return uavHandleGPU_; }
    // Vertex/Pixel Shaderにセットする用
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVHandleGPU() const { return srvHandleGPU_; }

    ID3D12Resource* GetResource() const { return resource_.Get(); } // バリア処理用に公開しておく

private:
    Fngine* p_fngine_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    uint32_t numElements_ = 0;
	// mappedData_ は無い！CPUからは直接触れない。

    D3D12_CPU_DESCRIPTOR_HANDLE uavHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE uavHandleGPU_{};
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_{};
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

        // --- UAVの作成 (書き込み用) ---
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.Format = format;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

        uavHandleCPU_ = fngine->GetSRV().GetCPUDescriptorHandle();
        uavHandleGPU_ = fngine->GetSRV().GetGPUDescriptorHandle();
        device->CreateUnorderedAccessView(resource_.Get(), nullptr, &uavDesc, uavHandleCPU_);

        // --- SRVの作成 (読み込み用) ---
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = format;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        srvHandleCPU_ = fngine->GetSRV().GetCPUDescriptorHandle();
        srvHandleGPU_ = fngine->GetSRV().GetGPUDescriptorHandle();
        device->CreateShaderResourceView(resource_.Get(), &srvDesc, srvHandleCPU_);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GetUAVHandleGPU() const { return uavHandleGPU_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVHandleGPU() const { return srvHandleGPU_; }
    ID3D12Resource* GetResource() const { return resource_.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;

    D3D12_CPU_DESCRIPTOR_HANDLE uavHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE uavHandleGPU_{};
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_{};
};

