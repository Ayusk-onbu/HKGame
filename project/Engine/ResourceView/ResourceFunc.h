#pragma once
#include <d3d12.h>
#include <cassert>
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "wrl.h"
#include <memory>

Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(ID3D12Device* device, size_t sizeInBytes, bool isUAV = false);

class ResourceObject {
public:
	ResourceObject(Microsoft::WRL::ComPtr<ID3D12Resource> resource) : resource_(std::move(resource)) {};
	ID3D12Resource* Get() { return resource_.Get(); }
private:
	Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
};