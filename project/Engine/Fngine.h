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
#include "SRVManager.h"
#include "OffScreenRendering.h"
#include "DirectionLight.h"
#include "PointLight.h"
#include "SpotLight.h"
#include "AreaLight.h"
#include "CameraForGPU.h"
#include "ImGuiManager.h"
#include "Structures.h"
#include "Constant.h"
#include "ResourceBarrier.h"
#include "InputManager.h"
#include "CameraSystem.h"
#include "RandomUtils.h"
#include "PauseSystem.h"
#include "Music.h"
#include "Easing.h"
#include "../UsefulTool/ISingleton.h"
#include "externals/DirectXTex/DirectXTex.h"
#include "../UsefulTool/EditorManager/EditorManager.h"

//#define pi float(3.14159265358979323846f)

class Hair;

class RWTexture2D;

struct PostEffectConfig
{
	Matrix4x4 projectionInverse;

	int enableVignette = 0;          
	int enableRadialBlur = 0;        
	int enableRandom = 0;            
	int enableLuminanceOutline = 1;  

	int enableGaussian = 0;          
	int enableDepthOutline = 0;      
	int enableBoxFilter = 0;
	int enableGrayscale = 0;

	float vignetteIntensity = 1.0f;  
	float radialBlurWidth = 0.01f;   
	float time = 0.0f;               
	float pad[1] = { 0.0f };   
};

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
	DXC& GetDXC() { return dxc_; }
	TachyonSync& GetTachyonSync() { return tachyonSync_; }

	DirectionLight& GetLight() { return light_; }
	PointLight& GetPointLight() { return pointLight_; }
	SpotLight& GetSpotLight() { return spotLight_; }
	AreaLight& GetAreaLight() { return areaLight_; }
	CameraForGPU& GetCameraForGPU() { return cameraForGPU_; }

	void ChangeOSRsDSVHandleType(DSV_HANDLE_TYPE type);
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

	RTV rtv_;
	DSV dsv_;
	// 複数のPSOを作らないといけないのでManagerを作成する
	DXC dxc_;
	RenderTargetGroup osrGroup_;
	D3D12_VIEWPORT viewport_;
	D3D12_RECT scissorRect_;
	SwapChain swapChain_;

	DirectionLight light_;
	PointLight pointLight_;
	SpotLight spotLight_;
	AreaLight areaLight_;

	CameraForGPU cameraForGPU_;


public:
	// 髪の毛
	std::unique_ptr<Hair> hair_ = nullptr;

	std::unique_ptr<RWTexture2D> intermediate_;
	std::unique_ptr<RWTexture2D> finalOutputColor_;

//////////////////
/// 
/// PostEffect用の物(他に引っ越すことを計画中)少なくともここは使いにくい
/// 
/////////////////
public:
	void SetUsePostEffect(const std::string& name) { usePostEffectName_ = name; }

	// --- 1. Box Filter (軽量ぼかし) ---
	void UseBoxFilter();

	// --- 2. Gaussian Blur (高品質ぼかし) ---
	void UseGaussian(){postPerfectForGPU_->GetMappedData()->enableGaussian = 1;}

	// --- 3. Grayscale (モノクロ化) ---
	void UseGrayscale(){postPerfectForGPU_->GetMappedData()->enableGrayscale = 1;}

	// --- 4. Vignetting (画面端の減光・赤み) ---
	// intensity: 0.0f (変化なし) ～ 1.0f (最大減光)
	void UseVignette()
	{
		auto* data = postPerfectForGPU_->GetMappedData();
		data->enableVignette = 1;
		data->vignetteIntensity = 1.0f;
	}

	// --- 5. Radial Blur (放射状ぼかし) ---
	// width: ブラーの広がり幅 (デフォルト: 0.01f)
	void UseRadialBlur()
	{
		auto* data = postPerfectForGPU_->GetMappedData();
		data->enableRadialBlur = 1;
		data->radialBlurWidth = 0.01f;
	}

	// --- 6. Random (グリッチノイズ) ---
	void UseRandom();

	// --- 7. Luminance Based Outline (輝度輪郭) ---
	void UseLuminanceOutline()
	{
		postPerfectForGPU_->GetMappedData()->enableLuminanceOutline = 1;
	}

	// --- 8. Depth Based Outline (深度輪郭・トゥーン調) ---
	// projectionInverse: カメラの逆プロジェクション行列
	void UseDepthOutline()
	{
		auto* data = postPerfectForGPU_->GetMappedData();
		data->enableDepthOutline = 1;
	}
private:
	std::string usePostEffectName_ = "PostPerfect";

	std::unique_ptr<ConstantBuffer<PostEffectConfig>> postPerfectForGPU_;
	int hitTimeIndex_ = -1;

	std::unique_ptr<ConstantBuffer<OutlineForGPU>> outlineForGPU_;
public:
	std::unique_ptr<ConstantBuffer<DissolveConfigForGPU>> dissolveForGPU_;

	std::unique_ptr<ConstantBuffer<RandomConfigForGPU>> randomForGPU_;
};

