#include "D3D12ResourceLeakChecker.h"
#include <dxgidebug.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

D3D12ResourceLeakChecker::~D3D12ResourceLeakChecker() {
	// リソースリーク✅
	Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
	//リソースリークチェック==================-↓↓↓
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		//debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
		//debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		//debug->Release();
	}
	//リソースリークチェック==================-↑↑↑
}