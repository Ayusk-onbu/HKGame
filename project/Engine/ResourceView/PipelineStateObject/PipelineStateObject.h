#pragma once
#include "Log.h"
#include "RootSignature.h"
#include "InputLayout.h"
#include "BlendState.h"
#include "RasterizerState.h"
#include "DepthStencil.h"
#include "DXC.h"

class Fngine;

enum class PIPELINETYPE {
	Graphics,// これは VS, PS, (GS, DS, HS etc...)
	Compute  // CS のみに活用
};

enum class PSOTYPE {
	Normal,
	Line,
	Skinning,
	CopyImage,
};

struct ShaderCompileSettings {
	// グラフィックス用のシェーダー
	std::wstring vsFilePath;
	std::wstring psFilePath;
	// ( GS, HS, DS も追加して良い )

	// コンピュート用シェーダー
	std::wstring csFilePath;

	// プロファイル
	const wchar_t* vsProfile = L"vs_6_0";
	const wchar_t* psProfile = L"ps_6_0";
	const wchar_t* csProfile = L"cs_6_0";
};

struct PSOKey {
	PIPELINETYPE pipelineType;// pipelineのType設定
	ROOTTYPE rootSignatureType;// rootSignatureのType設定
	PSOTYPE psoType;

	ShaderCompileSettings shaderCompileSettings;

	// ラスタライザ設定
	RasterizerSettings rasterizerSettings;

	// Depthの設定
	bool depthFlag;
};

class PipelineStateObject
{
public:
	//void Initialize(Fngine* fngine, PSOTYPE type);

	void Initialize(Fngine* fngine, const PSOKey& key);

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

	/// <summary>
	/// PSOのDescに情報をまとめる
	/// </summary>
	void MargeDesc();

	// --------------------------------
	// Set 
	// --------------------------------

	void SetDesc(D3D12System& d3d12, PSOTYPE type);

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

	PIPELINETYPE pipelineType_ = PIPELINETYPE::Graphics;

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