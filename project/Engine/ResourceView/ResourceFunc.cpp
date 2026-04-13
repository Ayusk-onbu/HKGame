#include "ResourceFunc.h"

Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(ID3D12Device* device, size_t sizeInBytes, bool isUAV) {
	Microsoft::WRL::ComPtr<ID3D12Resource> buffer = nullptr;
	// ヒーププロパティの設定
	//頂点リソース用のヒープの生成
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = isUAV ? D3D12_HEAP_TYPE_DEFAULT : D3D12_HEAP_TYPE_UPLOAD;// use UploadHeap
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	// リソースの記述
	//頂点リソースの設定
	D3D12_RESOURCE_DESC resourceDesc = {};
	//バッファリソース。テクさちゃの場合はまた別の設定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;//リソースのサイズ
	//バッファの場合は 1にしないといけない
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	//バッファはこれ
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	// UAVとして使う場合はフラグを追加
	resourceDesc.Flags = isUAV ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
	// make initial resource state
	D3D12_RESOURCE_STATES initialState = isUAV ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_GENERIC_READ;
	
	// リソースの作成
	HRESULT hr = device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		initialState,
		nullptr,
		IID_PPV_ARGS(buffer.GetAddressOf())
	);
	assert(SUCCEEDED(hr));
	return buffer;
}