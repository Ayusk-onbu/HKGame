#pragma once
#include <dxgi1_6.h>
#include <string>
#include <format>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <dxgidebug.h>
#include <cmath>
#include <wrl.h>
#include "Window.h"
#include "ErrorGuardian.h"
#include "Log.h"
#include "DXGI.h"
#include "D3D12System.h"
#include "TheOrderCommand.h"
#include "SwapChain.h"
#include "TachyonSync.h"
#include "OmnisTechOracle.h"
#include "RenderTargetView.h"
#include "Texture.h"
#include "PipelineStateObjectManager.h"
#include "OffScreenRendering.h"
#include "DirectionLight.h"
#include "PointLight.h"
#include "SpotLight.h"
#include "AreaLight.h"
#include "CameraForGPU.h"
#include "ImGuiManager.h"
#include "Structures.h"
#include "ResourceBarrier.h"
#include "InputManager.h"
#include "CameraSystem.h"
#include "RandomUtils.h"
#include "PauseSystem.h"
#include "Music.h"
#include "Easing.h"
#include "externals/DirectXTex/DirectXTex.h"

#define pi float(3.14159265358979323846f)

class Fngine
{
public:
	Fngine();
	~Fngine();
public:
	void Initialize();
	void BeginOSRFrame();
	void EndOSRFrame();
	void BeginFrame();
	void EndFrame();
public:
	D3D12System& GetD3D12System() { return d3d12_; }
	TheOrderCommand& GetCommand() { return command_; }
	SRV& GetSRV() { return srv_; }
	DXC& GetDXC() { return dxc_; }
	TachyonSync& GetTachyonSync() { return tachyonSync_; }

	DirectionLight& GetLight() { return light_; }
	PointLight& GetPointLight() { return pointLight_; }
	SpotLight& GetSpotLight() { return spotLight_; }
	AreaLight& GetAreaLight() { return areaLight_; }
	CameraForGPU& GetCameraForGPU() { return cameraForGPU_; }

	void ChangOSRsDSVHandleType(DSV_HANDLE_TYPE type);
private:
	void SettingShader();
private:

	int32_t kClienWidth_ = 1280;
	int32_t kClienHeight_ = 720;

private:

	Window window_;
	D3D12System d3d12_;
	DXGI dxgi_;
	ErrorGuardian errorGuardian_;
	TheOrderCommand command_;
	OmnisTechOracle omnisTechOracle_;
	TachyonSync tachyonSync_;

	SRV srv_;
	RTV rtv_;
	DSV dsv_;
	// 複数のPSOを作らないといけないのでManagerを作成する
	DXC dxc_;
	OffScreenRendering osr_;
	D3D12_VIEWPORT viewport_;
	D3D12_RECT scissorRect_;
	SwapChain swapChain_;

	DirectionLight light_;
	PointLight pointLight_;
	SpotLight spotLight_;
	AreaLight areaLight_;

	CameraForGPU cameraForGPU_;
};

