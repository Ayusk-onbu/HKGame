#pragma once
#include <wrl.h>
#include <d3d12.h>
#include "DescriptorHeap.h"
#include "D3D12System.h"

struct DepthSettings {
	bool depthEnable = true;
	D3D12_DEPTH_WRITE_MASK writeMask = D3D12_DEPTH_WRITE_MASK_ALL;
	D3D12_COMPARISON_FUNC comparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	DXGI_FORMAT formats = DXGI_FORMAT_D24_UNORM_S8_UINT; // フォーマットも指定可能に
};

class DepthStencil
{
public:

	void InitializeHeap(D3D12System& d3d12);

	void MakeResource(D3D12System& d3d12, int32_t width, int32_t height);

	void InitializeDesc(BOOL is = true, 
		D3D12_DEPTH_WRITE_MASK mask = D3D12_DEPTH_WRITE_MASK_ALL,
		D3D12_COMPARISON_FUNC func = D3D12_COMPARISON_FUNC_LESS_EQUAL);

	//DepthStencilTexture(奥行きの根幹をなすもの大量に読み書きするらしい)
	Microsoft::WRL::ComPtr < ID3D12Resource> CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height);

	D3D12_DEPTH_STENCIL_DESC GetDesc() const { return depthStencilDesc_; }
	D3D12_DEPTH_STENCIL_VIEW_DESC& GetDSVDesc() { return dsvDesc_; }
	DescriptorHeap GetHeap() { return heap_; }
	Microsoft::WRL::ComPtr <ID3D12Resource>& GetResource() { return depthStencilResource_; }
private:
	/// <summary>
	/// 深度テストを使うか否かの設定
	/// </summary>
	/// <param name="is"></param>
	void SetEnable(BOOL is) { depthStencilDesc_.DepthEnable = is; }
	/// <summary>
	/// Zバッファに深度情報を書き込むか決める
	/// </summary>
	/// <param name="mask"></param>
	void SetMask(D3D12_DEPTH_WRITE_MASK mask) { depthStencilDesc_.DepthWriteMask = mask; }
	/// <summary>
	/// Z値の比較演算子の設定
	/// </summary>
	/// <param name="func"></param>
	void SetFunc(D3D12_COMPARISON_FUNC func) { depthStencilDesc_.DepthFunc = func; }

	void SetDesc() {
		dsvDesc_.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc_.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	}
private:
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc_{};
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc_{};
	DescriptorHeap heap_;
	Microsoft::WRL::ComPtr <ID3D12Resource> depthStencilResource_;
};

using DSV = DepthStencil;

