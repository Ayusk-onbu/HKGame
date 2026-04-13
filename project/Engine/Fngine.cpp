#include "Fngine.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "Chronos.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"dxguid.lib")//0103 ReportLiveobject

Fngine::Fngine() {

}

Fngine::~Fngine() {
	ImGuiManager::GetInstance()->Shutdown();

	TextureManager::GetInstance()->ReleaseInstance();

	ModelManager::GetInstance()->ReleaseInstance();

	Music::GetInstance()->UnLoad();

	PSOManager::GetInstance()->ReleaseInstance();
	//解放処理
	tachyonSync_.GetCGPU().UnLoad();


#ifdef _DEBUG
	//debugController->Release();
#endif // _DEBUG
	CloseWindow(window_.GetHwnd());
}

void Fngine::SettingShader() {
	PSOManager::GetInstance()->CreateNewPSO
	(
		{
			PIPELINETYPE::Graphics,
			ROOTTYPE::Structured,
			PSOTYPE::Normal,
		 {
			L"resources/shaders/Particle/Particle.VS.hlsl",
			L"resources/shaders/Particle/Particle.PS.hlsl",
			L"",
			L"vs_6_0",
			L"ps_6_0",
			L""
		 },
		 {
			 D3D12_CULL_MODE_BACK,
			 D3D12_FILL_MODE_SOLID,
			 FALSE,
			 0,
			 0.0f
		 },
		 false
		},
		"Structured"
	);
	PSOManager::GetInstance()->GetPSO("Structured").SetBlendState(BLENDMODE::Additive);

	PSOManager::GetInstance()->CreateNewPSO
	(
		{
			PIPELINETYPE::Graphics,
			ROOTTYPE::Normal,
			PSOTYPE::Normal,
		 {
			L"resources/shaders/Object3D/Object3D.VS.hlsl",
			L"resources/shaders/Object3D/Object3D.PS.hlsl",
			L"",
			L"vs_6_0",
			L"ps_6_0",
			L""
		 },
		 {
			 D3D12_CULL_MODE_BACK,
			 D3D12_FILL_MODE_SOLID,
			 FALSE,
			 0,
			 0.0f
		 },
		true
		},
		"Object3D"
	);

	PSOManager::GetInstance()->CreateNewPSO
	(
		{
			PIPELINETYPE::Graphics,
			ROOTTYPE::Normal,
			PSOTYPE::Normal,
		 {
			L"resources/shaders/Object3D/Object3D.VS.hlsl",
			L"resources/shaders/Object3D/Object3D.PS.hlsl",
			L"",
			L"vs_6_0",
			L"ps_6_0",
			L""
		 },
		 {
			 D3D12_CULL_MODE_BACK,
			 D3D12_FILL_MODE_WIREFRAME,
			 FALSE,
			 0,
			 0.0f
		 },
		true
		},
		"DebugObject3D"
	);

	PSOManager::GetInstance()->CreateNewPSO
	(
		{
			PIPELINETYPE::Graphics,
			ROOTTYPE::Normal,
			PSOTYPE::Normal,
		 {
			L"resources/shaders/Object3D/Object3D.VS.hlsl",
			L"resources/shaders/Object3D/Object3D.PS.hlsl",
			L"",
			L"vs_6_0",
			L"ps_6_0",
			L""
		 },
		 {
			 D3D12_CULL_MODE_NONE,
			 D3D12_FILL_MODE_SOLID,
			 FALSE,
			 0,
			 0.0f
		 },
		true
		},
		"SpriteObject3D"
	);

	/*PSOManager::GetInstance()->CreateNewPSO
	(
		{
			PIPELINETYPE::Graphics,
			ROOTTYPE::Skinning,
			PSOTYPE::Skinning,
		 {
			L"resources/shaders/SkinningObject3D/SkinningObject3D.VS.hlsl",
			L"resources/shaders/SkinningObject3D/SkinningObject3D.PS.hlsl",
			L"",
			L"vs_6_0",
			L"ps_6_0",
			L""
		 },
		 {
			 D3D12_CULL_MODE_BACK,
			 D3D12_FILL_MODE_SOLID,
			 FALSE,
			 0,
			 0.0f
		 },
		true
		},
		"SkinningObject3D"
	);*/

	PSOManager::GetInstance()->CreateNewPSO
	(
		{
			PIPELINETYPE::Graphics,
			ROOTTYPE::CopyImage,
			PSOTYPE::CopyImage,
		 {
			L"resources/shaders/CopyImage/CopyImage.VS.hlsl",
			L"resources/shaders/CopyImage/CopyImage.PS.hlsl",
			L"",
			L"vs_6_0",
			L"ps_6_0",
			L""
		 },
		 {
			 D3D12_CULL_MODE_BACK,
			 D3D12_FILL_MODE_SOLID,
			 FALSE,
			 0,
			 0.0f
		 },
		false
		},
		"CopyImage"
	);
	PSOManager::GetInstance()->GetPSO("CopyImage").SetBlendState(BLENDMODE::Multiplicative);

	PSOManager::GetInstance()->CreateNewPSO
	(
		{
			PIPELINETYPE::Compute,
			ROOTTYPE::Skinning,
			PSOTYPE::Skinning,
		 {
			L"",
			L"",
			L"resources/shaders/CS/Skinning.CS.hlsl",
			L"",
			L"",
			L"cs_6_0"
		 },
		 {
			 D3D12_CULL_MODE_NONE,
			 D3D12_FILL_MODE_SOLID,
			 FALSE,
			 0,
			 0.0f
		 },
		false
		},
		"SkinningCS"
	);
}

void Fngine::Initialize() {

	//COMの初期化(Windowsが提供している機能)
	CoInitializeEx(0, COINIT_MULTITHREADED);

	//誰も補足しなかった場合に（Unhandled）、補足する関数を登録
	//main関数はじまってすぐに登録するといい
	SetUnhandledExceptionFilter(ErrorGuardian::ExportDump);
	// Logの初期化
	Log::Initialize();

	window_.Initialize(L"CG2ClassName", L"LE2B_19_ハマダ_カズヤ_NONAME");

	errorGuardian_.SetDebugInterface();
	dxgi_.RecruitEngineer();
	omnisTechOracle_.Oracle(dxgi_);
	d3d12_.SelectLevel(omnisTechOracle_);
	errorGuardian_.SetQueue(d3d12_.GetDevice());
	command_.GetQueue().SetDescD();
	HRESULT hr;
	hr = d3d12_.GetDevice()->CreateCommandQueue(&command_.GetQueue().GetDesc(), IID_PPV_ARGS(&command_.GetQueue().GetQueue()));
	if (FAILED(hr)) {
		Log::ViewFile("CreateCommandQueue failed!!!\n");
	}
	hr = d3d12_.GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&command_.GetList().GetAllocator()));
	if (FAILED(hr)) {
		Log::ViewFile("CreateCommandAllocator failed!!!\n");
	}
	hr = d3d12_.GetDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_.GetList().GetAllocator().Get(), nullptr,
		IID_PPV_ARGS(&command_.GetList().GetList()));
	if (FAILED(hr)) {
		Log::ViewFile("CreateCommandList failed!!!\n");
	}
	swapChain_.Initialize(window_);
	dxgi_.AssignTaskToEngineer(command_.GetQueue().GetQueue(), window_, swapChain_);
	swapChain_.MakeResource();
	srv_.InitializeHeap(d3d12_);
	rtv_.Initialize(&d3d12_, swapChain_);
	dsv_.InitializeHeap(d3d12_);
	dsv_.MakeResource(d3d12_, kClienWidth_, kClienHeight_);
	d3d12_.GetDevice()->CreateDepthStencilView(dsv_.GetResource().Get(), &dsv_.GetDSVDesc(), dsv_.GetHeap().GetHeap()->GetCPUDescriptorHandleForHeapStart());
	tachyonSync_.GetCGPU().Initialize(d3d12_.GetDevice());


	dxc_.Initialize();

	PSOManager::GetInstance()->Initialize(this);
	SettingShader();

	osr_.Initialize(d3d12_, srv_, float(kClienWidth_), float(kClienHeight_));

	// こいつらはなに？キモい
	viewport_.Width = static_cast<float>(window_.GetWindowRect().right);
	viewport_.Height = static_cast<float>(window_.GetWindowRect().bottom);
	viewport_.TopLeftX = 0;
	viewport_.TopLeftY = 0;
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;

	scissorRect_.left = 0;
	scissorRect_.right = window_.GetWindowRect().right;
	scissorRect_.top = 0;
	scissorRect_.bottom = window_.GetWindowRect().bottom;

	InputManager::Initialize(window_.GetWindowClass(), window_.GetHwnd());
	ImGuiManager::GetInstance()->SetImGui(window_.GetHwnd(), d3d12_.GetDevice().Get(), srv_.GetDescriptorHeap().GetHeap().Get());
	ModelManager::GetInstance()->Initialize(this);
	TextureManager::GetInstance()->Initialize(*this);
	Music::GetInstance()->Initialize();
	Chronos::GetInstance()->Initialize();
	RandomUtils::GetInstance()->Initialize();

	light_.Initialize(d3d12_);
	pointLight_.Initialize(this);
	spotLight_.Initialize(this);
	areaLight_.Initialize(this);
	cameraForGPU_.Initialize(this);
}

void Fngine::BeginOSRFrame() {
	ResourceBarrier barrierO = {};
	barrierO.SetBarrier(command_.GetList().GetList().Get(), osr_.GetResource().Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
	osr_.Begin(command_);
	ID3D12DescriptorHeap* descriptorHeaps[] = { srv_.GetDescriptorHeap().GetHeap().Get() };
	command_.GetList().GetList()->SetDescriptorHeaps(1, descriptorHeaps);
	/////////////////////////////////////////////////////////////////////////
	//描画0200
	command_.GetList().GetList()->RSSetViewports(1, &viewport_);
	command_.GetList().GetList()->RSSetScissorRects(1, &scissorRect_);
}

void Fngine::EndOSRFrame() {
	//osr_.End(command_);
	ResourceBarrier barrier = {};
	//barrier.SetTransition(command_.GetList().GetList().Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	barrier.SetBarrier(command_.GetList().GetList().Get(), osr_.GetResource().Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
}

void Fngine::BeginFrame() {
	UINT backBufferIndex = swapChain_.GetSwapChain()->GetCurrentBackBufferIndex();

	ResourceBarrier barrier = {};
	barrier.SetBarrier(command_.GetList().GetList().Get(), swapChain_.GetResource(backBufferIndex).Get(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	//描画先のRTVとDSVを設定する
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsv_.GetHeap().GetHeap()->GetCPUDescriptorHandleForHeapStart();
	command_.GetList().GetList()->OMSetRenderTargets(1, &rtv_.GetHandle(backBufferIndex), false, &dsvHandle);
	//指定した色で画面全体をクリアする
	float clearColor[] = { 1.0f,1.0f,1.0f,0.0f };//RGBAの設定
	command_.GetList().GetList()->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);//
	command_.GetList().GetList()->ClearRenderTargetView(rtv_.GetHandle(backBufferIndex), clearColor, 0, nullptr);

	ID3D12DescriptorHeap* descriptorHeaps[] = { srv_.GetDescriptorHeap().GetHeap().Get() };
	command_.GetList().GetList()->SetDescriptorHeaps(1, descriptorHeaps);
	/////////////////////////////////////////////////////////////////////////
	//描画0200
	command_.GetList().GetList()->RSSetViewports(1, &viewport_);
	command_.GetList().GetList()->RSSetScissorRects(1, &scissorRect_);
}

void Fngine::EndFrame() {
	command_.GetList().GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	command_.GetList().GetList()->SetGraphicsRootSignature(PSOManager::GetInstance()->GetPSO("CopyImage").GetRootSignature().GetRS().Get());
	command_.GetList().GetList()->SetPipelineState(PSOManager::GetInstance()->GetPSO("CopyImage").GetGPS().Get());
	//SRVのDescritorTableの先頭を設定。0はrootParameter[0]である
	command_.GetList().GetList()->SetGraphicsRootDescriptorTable(0, osr_.GetHandleGPU());
	command_.GetList().GetList()->DrawInstanced(3, 1, 0, 0);

	ImGuiManager::GetInstance()->EndFrame(command_.GetList().GetList());
	//barrierO.SetTransition(command.GetList().GetList().Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
	UINT backBufferIndex = swapChain_.GetSwapChain()->GetCurrentBackBufferIndex();
	ResourceBarrier barrier = {};
	//barrier.SetTransition(command_.GetList().GetList().Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	barrier.SetBarrier(command_.GetList().GetList().Get(), swapChain_.GetResource(backBufferIndex).Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	//画面表示できるようにするために
	//コマンドリストの内容を確定させる。全てのコマンドを積んでからCloseすること
	HRESULT hr = {};
	hr = command_.GetList().GetList()->Close();
	assert(SUCCEEDED(hr));

	////////////////////////////////////////////////////////////
#pragma region 積んだコマンドをキックする
	//GPUにコマンドリストの実行を行わさせる
	ID3D12CommandList* commandLists[] = { command_.GetList().GetList().Get() };
	command_.GetQueue().GetQueue()->ExecuteCommandLists(1, commandLists);


	//GPUとOSに画面の交換を行うように通知する
	swapChain_.GetSwapChain()->Present(1, 0);
	tachyonSync_.GetCGPU().Update(command_.GetQueue().GetQueue());
	Chronos::GetInstance()->Update();

	// ここにFPS固定するための処理を書く

	//次のフレーム用のコマンドリストを準備
	hr = command_.GetList().GetAllocator()->Reset();
	assert(SUCCEEDED(hr));
	hr = command_.GetList().GetList()->Reset(command_.GetList().GetAllocator().Get(), nullptr);
	assert(SUCCEEDED(hr));
#pragma endregion
	////////////////////////////////////////////////////////////
}