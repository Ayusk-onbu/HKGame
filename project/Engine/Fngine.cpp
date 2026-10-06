#include "Fngine.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "DrawManager.h"
#include "EventManager.h"
#include "Chronos.h"
#include "SDFManager.h"
#include "Hair/IHair.h"

#include <d3dx12.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"dxguid.lib")//0103 ReportLiveobject

Fngine::Fngine() {

}

Fngine::~Fngine() {
	ImGuiManager::GetInstance()->Shutdown();

	TextureManager::GetInstance()->ReleaseInstance();

	SRVManager::GetInstance()->ReleaseInstance();

	SDFManager::GetInstance()->ReleaseInstance();

	ModelManager::GetInstance()->ReleaseInstance();

	DrawManager::GetInstance()->ReleaseInstance();

	Music::GetInstance()->UnLoad();

	PSOManager::GetInstance()->ReleaseInstance();
	//解放処理
	tachyonSync_.GetCGPU().UnLoad();


#ifdef _DEBUG
	//debugController->Release();
#endif // _DEBUG
	CloseWindow(window_.GetHwnd());
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
	//srv_.InitializeHeap(d3d12_);

	SRVManager::GetInstance()->Initialize(d3d12_.GetDevice().Get(), 1024);
	auto srv = SRVManager::GetInstance();

	rtv_.Initialize(&d3d12_, swapChain_);
	dsv_.InitializeHeap(d3d12_);
	dsv_.MakeResource(d3d12_, kClienWidth_, kClienHeight_);
	//d3d12_.GetDevice()->CreateDepthStencilView(dsv_.GetResource().Get(), &dsv_.GetDSVDesc(), dsv_.GetHeap().GetHeap()->GetCPUDescriptorHandleForHeapStart());
	tachyonSync_.GetCGPU().Initialize(d3d12_.GetDevice());


	dxc_.Initialize();

	PSOManager::GetInstance()->Initialize(this);
	PSOManager::GetInstance()->LoadAllPSOsFromDirectory("resources/Data/PSO");

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
	ImGuiManager::GetInstance()->SetImGui(window_.GetHwnd(), d3d12_.GetDevice().Get(), srv->GetHeap());
	ModelManager::GetInstance()->Initialize(this);
	TextureManager::GetInstance()->Initialize(*this);
	Music::GetInstance()->Initialize();
	Chronos::GetInstance()->Initialize();
	RandomUtils::GetInstance()->Initialize();
	//EditorManager::GetInstance()->Initialize();

	light_.Initialize(d3d12_);
	pointLight_.Initialize(this);
	spotLight_.Initialize(this);
	areaLight_.Initialize(this);
	cameraForGPU_.Initialize(this);

	outlineForGPU_ = std::make_unique<ConstantBuffer<OutlineForGPU>>(this);
	outlineForGPU_->Initialize();

	dissolveForGPU_ = std::make_unique<ConstantBuffer<DissolveConfigForGPU>>(this);
	dissolveForGPU_->Initialize();

	randomForGPU_ = std::make_unique<ConstantBuffer<RandomConfigForGPU>>(this);
	randomForGPU_->Initialize();

	postPerfectForGPU_ = std::make_unique<ConstantBuffer<PostEffectConfig>>(this);
	postPerfectForGPU_->Initialize();

	postPerfectForGPU_->GetMappedData()->enableLuminanceOutline = 0;
	postPerfectForGPU_->GetMappedData()->enableDepthOutline = 0;

	osr_.Initialize(d3d12_, float(kClienWidth_), float(kClienHeight_));
	hair_ = std::make_unique<Hair>();

	EventManager::GetInstance()->RegisterAction(
		EVENTCATEGORY::EFFECT, 10, this, &Fngine::UseBoxFilter
	);
	EventManager::GetInstance()->RegisterAction(
		EVENTCATEGORY::EFFECT, 11, this, &Fngine::UseRandom
	);
	EventManager::GetInstance()->RegisterAction(
		EVENTCATEGORY::EFFECT, 12, this, &Fngine::UseRadialBlur
	);
	EventManager::GetInstance()->RegisterAction(
		EVENTCATEGORY::EFFECT, 13, this, &Fngine::UseVignette
	);
	EventManager::GetInstance()->RegisterAction(
		EVENTCATEGORY::EFFECT, 15, this, &Fngine::UseGrayscale
	);

	EventManager::GetInstance()->BindEventToTag(GAMEEVENTID::PlayerTakeDamage, EVENTCATEGORY::EFFECT, 10);
	EventManager::GetInstance()->BindEventToTag(GAMEEVENTID::PlayerTakeDamage, EVENTCATEGORY::EFFECT, 11);
	EventManager::GetInstance()->BindEventToTag(GAMEEVENTID::PlayerDash, EVENTCATEGORY::EFFECT, 12);
	EventManager::GetInstance()->BindEventToTag(GAMEEVENTID::PlayerOMG, EVENTCATEGORY::EFFECT, 13);
	//EventManager::GetInstance()->BindEventToTag(GAMEEVENTID::PlayerOMG, EVENTCATEGORY::EFFECT, 14);
	EventManager::GetInstance()->BindEventToTag(GAMEEVENTID::PlayerDie, EVENTCATEGORY::EFFECT, 15);
}

void Fngine::UseBoxFilter() { 
	if (hitTimeIndex_ == -1) {
		hitTimeIndex_ = TimeKeeper::GetInstance()->RegisterTimer(0.1f, 0.0f, false);
	}
}

void Fngine::UseRandom() { 
	
}

void Fngine::BeginOSRFrame() {
	postPerfectForGPU_->GetMappedData()->enableVignette = 0;
	postPerfectForGPU_->GetMappedData()->enableRadialBlur = 0;
	postPerfectForGPU_->GetMappedData()->enableGaussian = 0;
	postPerfectForGPU_->GetMappedData()->enableGrayscale = 0;

	if (hitTimeIndex_ != -1) {
		postPerfectForGPU_->GetMappedData()->enableBoxFilter = 1;
		postPerfectForGPU_->GetMappedData()->enableRandom = 1;
		if (TimeKeeper::GetInstance()->IsEnd(hitTimeIndex_)) {
			hitTimeIndex_ = -1;
		}
	}
	else {
		postPerfectForGPU_->GetMappedData()->enableBoxFilter = 0;
		postPerfectForGPU_->GetMappedData()->enableRandom = 0;
	}

	ResourceBarrier barrierO = {};
	barrierO.SetBarrier(command_.GetList().GetList().Get(), osr_.GetResource().Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	osr_.Begin(command_);
	ID3D12DescriptorHeap* descriptorHeaps[] = { SRVManager::GetInstance()->GetHeap()};
	command_.GetList().GetList()->SetDescriptorHeaps(1, descriptorHeaps);
	/////////////////////////////////////////////////////////////////////////
	//描画0200
	command_.GetList().GetList()->RSSetViewports(1, &viewport_);
	command_.GetList().GetList()->RSSetScissorRects(1, &scissorRect_);
}

void Fngine::EndOSRFrame() {
	//osr_.End(command_);

	hair_->PreHair(osr_.GetResource().Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,osr_.GetDSVResource().Get());
	hair_->Render(osr_.GetDSVDepthHandleGPU());
	hair_->PostHair(osr_.GetResource().Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, osr_.GetDSVResource().Get());

	ResourceBarrier barrier = {};
	//barrier.SetTransition(command_.GetList().GetList().Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	barrier.SetBarrier(command_.GetList().GetList().Get(), osr_.GetResource().Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void Fngine::BeginFrame() {
	UINT backBufferIndex = swapChain_.GetSwapChain()->GetCurrentBackBufferIndex();

	ResourceBarrier barrier = {};
	barrier.SetBarrier(command_.GetList().GetList().Get(), swapChain_.GetResource(backBufferIndex).Get(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	//描画先のRTVとDSVを設定する
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsv_.GetCPUHandle(DSV_HANDLE_TYPE::Normal);
	command_.GetList().GetList()->OMSetRenderTargets(1, &rtv_.GetHandle(backBufferIndex), false, &dsvHandle);

	//指定した色で画面全体をクリアする
	float clearColor[] = { 1.0f,1.0f,1.0f,0.0f };//RGBAの設定
	command_.GetList().GetList()->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);//
	command_.GetList().GetList()->ClearRenderTargetView(rtv_.GetHandle(backBufferIndex), clearColor, 0, nullptr);

	ID3D12DescriptorHeap* descriptorHeaps[] = { SRVManager::GetInstance()->GetHeap() };
	command_.GetList().GetList()->SetDescriptorHeaps(1, descriptorHeaps);
	/////////////////////////////////////////////////////////////////////////
	//描画0200
	command_.GetList().GetList()->RSSetViewports(1, &viewport_);
	command_.GetList().GetList()->RSSetScissorRects(1, &scissorRect_);
}

void Fngine::EndFrame() {
	// Pre 
	if (usePostEffectName_ == "DepthBasedOutline"){
		auto transitionDepth = CD3DX12_RESOURCE_BARRIER::Transition(
			osr_.GetDSVResource().Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,                  // 元の状態
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE      // レイトレ(Compute)で読める状態
		);
		command_.GetList().GetList()->ResourceBarrier(1, &transitionDepth);

		outlineForGPU_->GetMappedData()->viewProjInverse = Matrix4x4::Inverse(CameraSystem::GetInstance()->GetActiveCamera()->GetViewProjectionMatrix());
	}
	else if (usePostEffectName_ == "Random") {
		randomForGPU_->GetMappedData()->time += 1.0f / 60.0f;
	}
	else if (usePostEffectName_ == "PostPerfect") {
		auto transitionDepth = CD3DX12_RESOURCE_BARRIER::Transition(
			osr_.GetDSVResource().Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,                  // 元の状態
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE      // レイトレ(Compute)で読める状態
		);
		command_.GetList().GetList()->ResourceBarrier(1, &transitionDepth);

		postPerfectForGPU_->GetMappedData()->projectionInverse = Matrix4x4::Inverse(CameraSystem::GetInstance()->GetActiveCamera()->GetViewProjectionMatrix());

		postPerfectForGPU_->GetMappedData()->time += Chronos::GetInstance()->GetGameDeltaTime();
	}

	command_.GetList().GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	command_.GetList().GetList()->SetGraphicsRootSignature(PSOManager::GetInstance()->GetPSO(usePostEffectName_).GetRootSignature().GetRS().Get());
	command_.GetList().GetList()->SetPipelineState(PSOManager::GetInstance()->GetPSO(usePostEffectName_).GetGPS().Get());
	//SRVのDescritorTableの先頭を設定。0はrootParameter[0]である
	command_.GetList().GetList()->SetGraphicsRootDescriptorTable(0, osr_.GetHandleGPU());
	if(usePostEffectName_ == "DepthBasedOutline")
	{
		command_.GetList().GetList()->SetGraphicsRootDescriptorTable(1, osr_.GetDSVDepthHandleGPU());
		command_.GetList().GetList()->SetGraphicsRootConstantBufferView(2, outlineForGPU_->GetGPUVirtualAddress());
	}
	else if (usePostEffectName_ == "Dissolve") {
		command_.GetList().GetList()->SetGraphicsRootDescriptorTable(1, TextureManager::GetInstance()->GetTexture("noise0").GetHandleGPU());
		command_.GetList().GetList()->SetGraphicsRootConstantBufferView(2, dissolveForGPU_->GetGPUVirtualAddress());
	}
	else if (usePostEffectName_ == "Random") {
		command_.GetList().GetList()->SetGraphicsRootConstantBufferView(1, randomForGPU_->GetGPUVirtualAddress());
	}
	else if (usePostEffectName_ == "PostPerfect") {
		command_.GetList().GetList()->SetGraphicsRootDescriptorTable(1, osr_.GetDSVDepthHandleGPU());
		command_.GetList().GetList()->SetGraphicsRootConstantBufferView(2, postPerfectForGPU_->GetGPUVirtualAddress());
	}

	command_.GetList().GetList()->DrawInstanced(3, 1, 0, 0);

	// Post
	if (usePostEffectName_ == "DepthBasedOutline")
	{
		auto depthTransition = CD3DX12_RESOURCE_BARRIER::Transition(
			osr_.GetDSVResource().Get(),
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_DEPTH_WRITE
		);
		command_.GetList().GetList()->ResourceBarrier(1, &depthTransition);
	}
	else if (usePostEffectName_ == "PostPerfect") {
		auto depthTransition = CD3DX12_RESOURCE_BARRIER::Transition(
			osr_.GetDSVResource().Get(),
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_DEPTH_WRITE
		);
		command_.GetList().GetList()->ResourceBarrier(1, &depthTransition);
	}

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
	auto allocator = command_.GetList().GetAllocator().Get();
	if (allocator) {
		hr = allocator->Reset();
		assert(SUCCEEDED(hr));
	}
	else {
		// ここで止まるなら、そもそも command_ の初期化に失敗しているか、
		// TextureManagerのどこかでアロケータ自体を破壊(Release)している。
		assert(false && "Allocator is missing!");
	}
	//hr = command_.GetList().GetAllocator()->Reset();
	//assert(SUCCEEDED(hr));
	hr = command_.GetList().GetList()->Reset(command_.GetList().GetAllocator().Get(), nullptr);
	assert(SUCCEEDED(hr));
#pragma endregion
	////////////////////////////////////////////////////////////
}

void Fngine::ChangOSRsDSVHandleType(DSV_HANDLE_TYPE type) {
	osr_.ChangeDSVHandleType(command_, type);
}