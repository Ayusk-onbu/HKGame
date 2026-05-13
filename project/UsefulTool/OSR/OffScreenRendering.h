#pragma once
#include <wrl.h>
#include "D3D12System.h"
#include "DescriptorHeap.h"
#include "ShaderResourceView.h"
#include "DepthStencil.h"
#include "PipelineStateObject.h"
#include "TheOrderCommand.h"

class OffRTV
{
public:

	void Initialize(D3D12System* d3d12, Microsoft::WRL::ComPtr <ID3D12Resource> resource,DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, D3D12_RTV_DIMENSION dimension = D3D12_RTV_DIMENSION_TEXTURE2D);

	void MakeDesc(DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, D3D12_RTV_DIMENSION dimension = D3D12_RTV_DIMENSION_TEXTURE2D);

	void MakeHandle(Microsoft::WRL::ComPtr <ID3D12Device> device, Microsoft::WRL::ComPtr <ID3D12Resource> resource);

	D3D12_CPU_DESCRIPTOR_HANDLE& GetHandle() { return handle_; }

private:
	DescriptorHeap descriptorHeap_;
	D3D12_RENDER_TARGET_VIEW_DESC desc_;
	D3D12_CPU_DESCRIPTOR_HANDLE startHandle_;
	D3D12_CPU_DESCRIPTOR_HANDLE handle_;

};

class OffScreenRendering
{
public:

	void Initialize(D3D12System& d3d12, SRV& srv, float width,float height,
		DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 
		D3D12_RTV_DIMENSION dimension = D3D12_RTV_DIMENSION_TEXTURE2D
		);
	void Begin(TheOrderCommand &command);
	void End(TheOrderCommand& command);
	D3D12_GPU_DESCRIPTOR_HANDLE& GetHandleGPU() { return textureSrvHandleGPU_; }
	Microsoft::WRL::ComPtr <ID3D12Resource> GetResource() {return offScreenTexture_;}
	void ChangeDSVHandleType(TheOrderCommand& command,DSV_HANDLE_TYPE type);
private:
	
private:
	Microsoft::WRL::ComPtr <ID3D12Resource> offScreenTexture_;
	OffRTV offRTV_;
	DSV dsv_;
	PSO pso_;
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc_{};
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU_;
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU_;
};

//// コマンドリストを取得 (例: d3d12.GetCommandList())
//ID3D12GraphicsCommandList* commandList = d3d12.GetCommandList();
//
//// D3D12_RESOURCE_STATE_COMMON (または現在の状態) から D3D12_RESOURCE_STATE_RENDER_TARGET へ遷移
//D3D12_RESOURCE_BARRIER barrier = {};
//barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
//barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
//barrier.Transition.pResource = renderTargetTexture_.Get();
//barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
//barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON; // もしくはテクスチャの現在の状態
//barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
//commandList->ResourceBarrier(1, &barrier);
//// レンダーターゲットをクリアする色 (例: 青)
//FLOAT clearColor[] = { 0.0f, 0.0f, 0.5f, 1.0f }; // R, G, B, A
//
//// コマンドリストにレンダーターゲットを設定
//// 深度ステンシルバッファを使用する場合は、DSVハンドルもここに渡す
//commandList->OMSetRenderTargets(1, &offscreenRTVHandle_, FALSE, nullptr); // FALSE は単一 RTV の場合
//
//// レンダーターゲットをクリア
//commandList->ClearRenderTargetView(offscreenRTVHandle_, clearColor, 0, nullptr);
//// D3D12_RESOURCE_STATE_RENDER_TARGET から D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE へ遷移
//// ポストプロセスなどでこのテクスチャをシェーダーから読み込む場合
//barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
//barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE; // または D3D12_RESOURCE_STATE_GENERIC_READ
//commandList->ResourceBarrier(1, &barrier);