#pragma once
#include <d3d12.h>
#include <cassert>
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "wrl.h"


Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(ID3D12Device* device, size_t sizeInBytes, bool isUAV = false);

class ResourceObject {
public:
	ResourceObject(ID3D12Resource* resource) : resource_(resource) {};
	// デストラクタはオブジェクトの寿命が尽きたときに呼ばれる
	~ResourceObject() {
		// ここでReleaseを呼ぶ
		if (resource_) {
			resource_->Release();
		}
	}
	ID3D12Resource* Get() { return resource_; }
private:
	ID3D12Resource* resource_;
};