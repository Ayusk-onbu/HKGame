#pragma once
#include "Log.h"
#include "RootSignature.h"
#include "InputLayout.h"
#include "BlendState.h"
#include "RasterizerState.h"
#include "DepthStencil.h"
#include "DXC.h"
#include <array>

class Fngine;

struct RenderTargetSettings {
	UINT numRenderTargets = 1;
	std::array<DXGI_FORMAT, 8> rtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB };
};

class PipelineStateObject
{
public:
	void InitializeDirectly(
		Fngine* engine, 
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature,
		const D3D12_INPUT_ELEMENT_DESC* inputElement, UINT numInputElement,
		BLENDMODE startBlendMode,
		std::wstring vsPath, std::wstring psPath,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType,
		RasterizerSettings rasterizerSettings,
		DepthSettings depthSettings,
		RenderTargetSettings renderTargetSettings = {}
		);

	void InitializeDirectlyCompute(
		Fngine* engine,
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature,
		const D3D12_INPUT_ELEMENT_DESC* inputElement, UINT numInputElement,
		std::wstring csPath
	);

	void Compile(
		//CompilerするShaderファイルへのパス
		const std::wstring& vsFilePath,
		//Compilerに使用するProfile
		const wchar_t* vsProfile,
		//CompilerするShaderファイルへのパス
		const std::wstring& psFilePath,
		//Compilerに使用するProfile
		const wchar_t* psProfile);

	Microsoft::WRL::ComPtr < IDxcBlob> CompileShader(
		//CompilerするShaderファイルへのパス
		const std::wstring& filePath,
		//Compilerに使用するProfile
		const wchar_t* profile,
		//初期化で生成したものを三つ
		IDxcUtils* dxcUtils,
		IDxcCompiler3* dxcCompiler,
		IDxcIncludeHandler* includeHandler);

	// --------------------------------
	// Set 
	// --------------------------------

	RootSignature GetRootSignature() { return rootSignature_; }
	InputLayout GetInputLayout() { return inputLayoutDesc_; }
	BlendState GetBlendState() { return blendState_; }
	void SetBlendState(BLENDMODE blendMode) { blendMode_ = blendMode; }
	RasterizerState  GetRasterizer() { return rasterizer_; }
	Microsoft::WRL::ComPtr<IDxcBlob>& GetVSB() { return vertexShaderBlob_; }
	Microsoft::WRL::ComPtr<IDxcBlob>& GetPSB() { return pixelShaderBlob_; }
	Microsoft::WRL::ComPtr <ID3D12PipelineState>& GetGPS();
	Microsoft::WRL::ComPtr <ID3D12PipelineState>& GetCPS() { return computePipelineState_; }

private:
	DXC* dxc_;
	RootSignature rootSignature_;
	InputLayout inputLayoutDesc_;
	BlendState blendState_;
	RasterizerState rasterizer_;
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob_;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob_;

	DepthStencil depthStencil_;

	// ブレンドモード
	BLENDMODE blendMode_ = BLENDMODE::AlphaBlend;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc_ = {};
	Microsoft::WRL::ComPtr <ID3D12PipelineState> graphicsPipelineState_ = nullptr;
	Microsoft::WRL::ComPtr <ID3D12PipelineState> graphicsPipelineState_Add = nullptr;
	Microsoft::WRL::ComPtr <ID3D12PipelineState> graphicsPipelineState_Sub = nullptr;
	Microsoft::WRL::ComPtr <ID3D12PipelineState> graphicsPipelineState_Mul = nullptr;
	Microsoft::WRL::ComPtr <ID3D12PipelineState> graphicsPipelineState_Scr = nullptr;

	//ComputePipeline
	Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob_;
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc_ = {};
	Microsoft::WRL::ComPtr <ID3D12PipelineState> computePipelineState_ = nullptr;
};

using PSO = PipelineStateObject;

class PSOBuilder {
public:
	PSOBuilder(Fngine* fngine, const std::string& psoName);

	// 各種設定をチェーンで繋ぐための関数群
	PSOBuilder& SetPipelineType(const std::string& typeName);
	PSOBuilder& SetShaders(const std::wstring& vsPath, const std::wstring& psPath);
	PSOBuilder& SetComputeShader(const std::wstring& csPath);
	PSOBuilder& SetInputLayout(const D3D12_INPUT_ELEMENT_DESC* pElements, UINT numElements);
	PSOBuilder& SetBlendMode(BLENDMODE mode);
	PSOBuilder& SetTopologyType(const D3D12_PRIMITIVE_TOPOLOGY_TYPE& type);
	PSOBuilder& SetRasterizerSettings(const RasterizerSettings& settings);
	PSOBuilder& SetDepthSettings(const DepthSettings& settings);
	PSOBuilder& SetRenderTargetSettings(const RenderTargetSettings& settings);

	// RootSignatureの設定もここにパススルーする
	PSOBuilder& AddCBV(UINT reg, D3D12_SHADER_VISIBILITY vis) { rsBuilder_.AddCBV(reg, vis); return *this; }
	PSOBuilder& AddSRVTable(UINT reg, UINT num, D3D12_SHADER_VISIBILITY vis) { rsBuilder_.AddSRVTable(reg, num, vis); return *this; }
	PSOBuilder& AddUAVTable(UINT reg, UINT num, D3D12_SHADER_VISIBILITY vis) { rsBuilder_.AddUAVTable(reg, num, vis); return *this; }
	PSOBuilder& AddStaticSampler(UINT reg, D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,D3D12_TEXTURE_ADDRESS_MODE u = D3D12_TEXTURE_ADDRESS_MODE_WRAP,D3D12_TEXTURE_ADDRESS_MODE v = D3D12_TEXTURE_ADDRESS_MODE_WRAP,D3D12_TEXTURE_ADDRESS_MODE w = D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_PIXEL) { rsBuilder_.AddStaticSampler(reg, filter,u,v,w, visibility); return *this; }
	PSOBuilder& AddConstants(UINT num32BitValues, UINT reg, D3D12_SHADER_VISIBILITY vis) {
		rsBuilder_.AddConstants(num32BitValues, reg, vis);
		return *this;
	}

	// 最後にこれを呼ぶとManagerに登録される
	void Build();

private:
	Fngine* fngine_;
	std::string name_;

	// 詰め込むデータ群 (現状のPSOKeyに入っていたようなもの)
	std::string pipelineType_;
	std::wstring vsPath_, psPath_, csPath_;
	const D3D12_INPUT_ELEMENT_DESC* pInputElements_ = nullptr;
	std::vector<D3D12_INPUT_ELEMENT_DESC>inputElements_;
	UINT numInputElements_ = 0;
	BLENDMODE blendMode_ = BLENDMODE::AlphaBlend;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType_;
	RasterizerSettings rasterizerSettings_;
	DepthSettings depthSettings_;
	RenderTargetSettings renderTargetSettings_;

	RootSignatureBuilder rsBuilder_;
};