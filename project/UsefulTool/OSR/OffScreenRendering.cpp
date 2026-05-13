#include "OffScreenRendering.h"
#include<dxgi1_6.h>
#include <d3d12.h>
#include <cassert>
#include "ResourceBarrier.h"

void OffScreenRendering::Initialize(D3D12System& d3d12, SRV& srv, float width, float height,
	DXGI_FORMAT fmt,D3D12_RTV_DIMENSION dimension) {
	

	D3D12_RESOURCE_DESC renderingDesc{};
	//   テクスチャの種類
	renderingDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	//   解像度に関する
	// 幅
	renderingDesc.Width = UINT(width);
	// 高さ
	renderingDesc.Height = UINT(height);
	//   Z　or　配列のサイズ
	renderingDesc.DepthOrArraySize = 1;
	//   何個同じリソースを作るか
	// ミップマップレベルの数数に応じて解像度が小さいリソースが用意される
	// Renderingするので１でいい
	renderingDesc.MipLevels = 1;
	//   ピクセルのフォーマット
	renderingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	//   サンプル数
	renderingDesc.SampleDesc.Count = 1;
	//   リソースのフラグ
	renderingDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = renderingDesc.Format;
	clearValue.Color[0] = 0.0f;
	clearValue.Color[1] = 0.0f;
	clearValue.Color[2] = 0.0f;
	clearValue.Color[3] = 0.0f;

	//   利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	HRESULT hr = d3d12.GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&renderingDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&clearValue,
		IID_PPV_ARGS(&offScreenTexture_));
	assert(SUCCEEDED(hr));

	offRTV_.Initialize(&d3d12, offScreenTexture_, fmt, dimension);

	//////////////////// ここまでがRTVの設定 ///////////////////////////

	srvDesc_.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	//ZeroMemory(&srvDesc_, sizeof(srvDesc_));
	srvDesc_.Format = offScreenTexture_->GetDesc().Format;
	srvDesc_.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc_.Texture2D.MostDetailedMip = 0;
	srvDesc_.Texture2D.MipLevels = renderingDesc.MipLevels;
	textureSrvHandleCPU_ = srv.GetCPUDescriptorHandle();
	textureSrvHandleGPU_ = srv.GetGPUDescriptorHandle();
	d3d12.GetDevice()->CreateShaderResourceView(offScreenTexture_.Get(), &srvDesc_, textureSrvHandleCPU_);


	dsv_.InitializeHeap(d3d12);
	dsv_.MakeResource(d3d12,int32_t(width),int32_t(height)); 
	//d3d12.GetDevice()->CreateDepthStencilView(dsv_.GetResource().Get(), &dsv_.GetDSVDesc(), dsv_.GetHeap().GetHeap()->GetCPUDescriptorHandleForHeapStart());

	//pso_.Initialize(, PSOTYPE::Normal);
	
}

void OffScreenRendering::Begin(TheOrderCommand& command) {
	
	// レンダーターゲットをクリアする色 (例: 青)
	FLOAT clearColor[] = { 0.0f,0.0f,0.0f,0.0f }; // R, G, B, A

	// コマンドリストにレンダーターゲットを設定
	// 深度ステンシルバッファを使用する場合は、DSVハンドルもここに渡す
	//command.GetList().GetList()->OMSetRenderTargets(1, &offRTV_.GetHandle(), FALSE, nullptr); // FALSE は単一 RTV の場合
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsv_.GetCPUHandle(DSV_HANDLE_TYPE::Normal);
	command.GetList().GetList()->OMSetRenderTargets(1, &offRTV_.GetHandle(), false, &dsvHandle);

	command.GetList().GetList()->ClearRenderTargetView(offRTV_.GetHandle(), clearColor, 0, nullptr);
	command.GetList().GetList()->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);//
}

void OffScreenRendering::End(TheOrderCommand& command) {
	//ResourceBarrier barrier = {};
	//barrier.SetTransition(command.GetList().GetList().Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void OffScreenRendering::ChangeDSVHandleType(TheOrderCommand& command,DSV_HANDLE_TYPE type) {
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsv_.GetCPUHandle(type);
	command.GetList().GetList()->OMSetRenderTargets(1, &offRTV_.GetHandle(), false, &dsvHandle);
}