#include "PipelineStateObject.h"
#include <cassert>
#include "Fngine.h"

void PipelineStateObject::Compile(
	//CompilerするShaderファイルへのパス
	const std::wstring& vsFilePath,
	//Compilerに使用するProfile
	const wchar_t* vsProfile,
	//CompilerするShaderファイルへのパス
	const std::wstring& psFilePath,
	//Compilerに使用するProfile
	const wchar_t* psProfile)
{
	vertexShaderBlob_ = CompileShader(vsFilePath,vsProfile, 
		dxc_->GetUtils().Get(), dxc_->GetCompiler().Get(), dxc_->GetIncludeHandle().Get());
	assert(vertexShaderBlob_ != nullptr);
	pixelShaderBlob_ = CompileShader(psFilePath,psProfile,
		dxc_->GetUtils().Get(), dxc_->GetCompiler().Get(), dxc_->GetIncludeHandle().Get());
	assert(pixelShaderBlob_ != nullptr);
}

//CompileShader関数
Microsoft::WRL::ComPtr < IDxcBlob> PipelineStateObject::CompileShader(
	//CompilerするShaderファイルへのパス
	const std::wstring& filePath,
	//Compilerに使用するProfile
	const wchar_t* profile,
	//初期化で生成したものを三つ
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler)
{
	// ファイルが空の場合は終了
	if (filePath.empty()) {
		return nullptr;
	}

	// hlslファイルを読む
	//これからシェーダーをコンパイルする旨をログに出す
	Log::View(ConvertString(std::format(L"Begin CompileShader,path:{}, profile:{}\n", filePath, profile)));
	
	//hlslファイルを読み込む
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	
	// パス間違いなどでファイルが開けなかった場合のログ出力
	if (FAILED(hr)) {
		Log::View(ConvertString(std::format(L"Failed to LoadFile: {}\n", filePath)));
		assert(false);
		return nullptr;
	}
	
	//読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;//UTF8の文字コードであることを通知
	
	// Compileする
	LPCWSTR arguments[] = {
		filePath.c_str(),
		L"-E",L"main",
		L"-T",profile,
		L"-Zi",L"-Qembed_debug",
		L"-Od",
		L"-Zpr",
	};
	//実際にShaderをコンパイルする
	Microsoft::WRL::ComPtr <IDxcResult> shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,
		arguments,
		_countof(arguments),
		includeHandler,
		IID_PPV_ARGS(&shaderResult)
	);
	//コンパイルエラーではなくdxcが起動できないなど致命的な状況
	assert(SUCCEEDED(hr));

	// 警告・エラーが惰ていないか確認する
	//警告・エラーが出ていたらログに出して止める
	Microsoft::WRL::ComPtr <IDxcBlobUtf8> shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Log::View(shaderError->GetStringPointer());
		//警告エラーダメ絶対
		assert(false);
	}
	
	// Compile結果を受け取って返す
	//コンパイル結果から実行用のバイナリ部分を取得
	Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	
	//成功したログを出す
	Log::View(ConvertString(std::format(L"CompileSucceeded, path:{},profile:{}\n", filePath, profile)));
	
	//実行用のバイナリを返却
	return shaderBlob;
}

Microsoft::WRL::ComPtr <ID3D12PipelineState>& PipelineStateObject::GetGPS() {
	if (blendMode_ == BLENDMODE::AlphaBlend) {
		return graphicsPipelineState_;
	}
	else if (blendMode_ == BLENDMODE::Additive) {
		return graphicsPipelineState_Add;
	}
	else if (blendMode_ == BLENDMODE::Subtractive) {
		return graphicsPipelineState_Sub;
	}
	else if (blendMode_ == BLENDMODE::Multiplicative) {
		return graphicsPipelineState_Mul;
	}
	else if (blendMode_ == BLENDMODE::ScreenBlend) {
		return graphicsPipelineState_Scr;
	}
	else {
		return graphicsPipelineState_;
	}
}

void PipelineStateObject::InitializeDirectly(
	Fngine* engine,
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature,
	const D3D12_INPUT_ELEMENT_DESC* inputElement, UINT numInputElement,
	BLENDMODE startBlendMode,
	std::wstring vsPath, std::wstring psPath,
	D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType,
	RasterizerSettings rasterizerSettings,
	DepthSettings depthSettings,
	RenderTargetSettings renderTargetSettings
	) 
{
	//////////////////////
	///
	/// 【 共通処理 】
	///
	//////////////////////

	// DXCを取得
	dxc_ = &engine->GetDXC();

	// 作成されたrootSignatureの情報を取得
	rootSignature_.GetRS() = std::move(rootSignature);

	inputLayoutDesc_.Initialize(inputElement, numInputElement);

	//
	blendState_.Initialize(USECOLOR::All);

	// ラスタライザ <= まだ
	rasterizer_.SetDesc(rasterizerSettings);

	vertexShaderBlob_ = CompileShader(
		vsPath, 
		L"vs_6_6",
		dxc_->GetUtils().Get(), 
		dxc_->GetCompiler().Get(), 
		dxc_->GetIncludeHandle().Get()
	);
	assert(vertexShaderBlob_ != nullptr);

	if (!psPath.empty()) {
		pixelShaderBlob_ = CompileShader(
			psPath,
			L"ps_6_6",
			dxc_->GetUtils().Get(), 
			dxc_->GetCompiler().Get(), 
			dxc_->GetIncludeHandle().Get()
		);
		assert(pixelShaderBlob_ != nullptr);
	}
	else {
		pixelShaderBlob_ = nullptr;
	}

	depthStencil_.InitializeDesc(depthSettings.depthEnable,
								depthSettings.writeMask,
								depthSettings.comparisonFunc);

	graphicsPipelineStateDesc_.pRootSignature = rootSignature_.GetRS().Get();//RootSignature
	graphicsPipelineStateDesc_.InputLayout = inputLayoutDesc_.GetDesc();//InputLayout
	graphicsPipelineStateDesc_.RasterizerState = rasterizer_.GetDesc();//RasterizerState
	
	graphicsPipelineStateDesc_.VS = { vertexShaderBlob_->GetBufferPointer(),
	vertexShaderBlob_->GetBufferSize() };// VertexShader
	
	if (pixelShaderBlob_ != nullptr) {
		graphicsPipelineStateDesc_.PS = { 
			pixelShaderBlob_->GetBufferPointer(),
			pixelShaderBlob_->GetBufferSize() 
		};//PixelShader
		//書きこむRTVの情報
		graphicsPipelineStateDesc_.NumRenderTargets = renderTargetSettings.numRenderTargets;
		for (UINT i = 0; i < renderTargetSettings.numRenderTargets; ++i) {
			graphicsPipelineStateDesc_.RTVFormats[i] = renderTargetSettings.rtvFormats[i];
		}
	}
	else {
		graphicsPipelineStateDesc_.PS = {
			nullptr,
			0
		};//PixelShader
		//書きこむRTVの情報
		graphicsPipelineStateDesc_.NumRenderTargets = 0;
		graphicsPipelineStateDesc_.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	}
	
	//利用するトボロジ（形状）のタイプ。三角形
	graphicsPipelineStateDesc_.PrimitiveTopologyType = topologyType;

	//どのように画面に色を打ち込むかの設定
	graphicsPipelineStateDesc_.SampleDesc.Count = 1;
	graphicsPipelineStateDesc_.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	//DepthStencil
	graphicsPipelineStateDesc_.DepthStencilState = depthStencil_.GetDesc();
	graphicsPipelineStateDesc_.DSVFormat = depthSettings.formats;

	// ---  --------------- ------------ ------ //
	//  BlendState                              //
	//  -------- -------- -------  -- --------- //
	auto& d3d12 = engine->GetD3D12System();
	HRESULT hr;

	if (pixelShaderBlob_ == nullptr) {
		blendState_.SetBlendMode(BLENDMODE::None);
		graphicsPipelineStateDesc_.BlendState = blendState_.GetDesc();

		hr = d3d12.GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc_,
			IID_PPV_ARGS(&graphicsPipelineState_));
		assert(SUCCEEDED(hr));
		return;
	}

	blendState_.SetBlendMode(startBlendMode);
	graphicsPipelineStateDesc_.BlendState = blendState_.GetDesc();//BlendState

	hr = d3d12.GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc_,
		IID_PPV_ARGS(&graphicsPipelineState_));
	assert(SUCCEEDED(hr));

	blendState_.SetBlendMode(BLENDMODE::Additive);//BlendStateの設定
	graphicsPipelineStateDesc_.BlendState = blendState_.GetDesc();//BlendState

	//実際に生成
	hr = d3d12.GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc_,
		IID_PPV_ARGS(&graphicsPipelineState_Add));
	assert(SUCCEEDED(hr));

	blendState_.SetBlendMode(BLENDMODE::Subtractive);//BlendStateの設定
	graphicsPipelineStateDesc_.BlendState = blendState_.GetDesc();//BlendState

	//実際に生成
	hr = d3d12.GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc_,
		IID_PPV_ARGS(&graphicsPipelineState_Sub));
	assert(SUCCEEDED(hr));

	blendState_.SetBlendMode(BLENDMODE::ScreenBlend);//BlendStateの設定
	graphicsPipelineStateDesc_.BlendState = blendState_.GetDesc();//BlendState

	//実際に生成
	hr = d3d12.GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc_,
		IID_PPV_ARGS(&graphicsPipelineState_Scr));
	assert(SUCCEEDED(hr));

	blendState_.SetBlendMode(BLENDMODE::Multiplicative);//BlendStateの設定
	graphicsPipelineStateDesc_.BlendState = blendState_.GetDesc();//BlendState

	//実際に生成
	hr = d3d12.GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc_,
		IID_PPV_ARGS(&graphicsPipelineState_Mul));
	assert(SUCCEEDED(hr));
}

void PipelineStateObject::InitializeDirectlyCompute(
	Fngine* engine,
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature,
	const D3D12_INPUT_ELEMENT_DESC* inputElement, UINT numInputElement,
	std::wstring csPath
) 
{
	//////////////////////
	///
	/// 【 共通処理 】
	///
	//////////////////////

	// DXCを取得
	dxc_ = &engine->GetDXC();

	// 作成されたrootSignatureの情報を取得
	rootSignature_.GetRS() = std::move(rootSignature);

	inputLayoutDesc_.Initialize(inputElement, numInputElement);

	// CSなら
	computeShaderBlob_ = CompileShader(csPath, L"cs_6_0",
		dxc_->GetUtils().Get(), dxc_->GetCompiler().Get(), dxc_->GetIncludeHandle().Get());
	assert(computeShaderBlob_ != nullptr);
	 
	// ルートシグネチャの情報を取得
	computePipelineStateDesc_.pRootSignature = rootSignature_.GetRS().Get();
	computePipelineStateDesc_.CS = {
		computeShaderBlob_->GetBufferPointer(),
		computeShaderBlob_->GetBufferSize()
	};

	auto d3d12 = engine->GetD3D12System();

	//実際に生成
	HRESULT hr;
	// 作成書をもとにPSOを生成
	hr = d3d12.GetDevice()->CreateComputePipelineState(&computePipelineStateDesc_,
		IID_PPV_ARGS(&computePipelineState_));
	assert(SUCCEEDED(hr));
}

///////////////////////
///
///  PSO Builder
///
///////////////////////

PSOBuilder::PSOBuilder(Fngine* fngine, const std::string& psoName)
	: fngine_(fngine), name_(psoName) {
}

PSOBuilder& PSOBuilder::SetPipelineType(const std::string& typeName) {
	pipelineType_ = typeName;
	return *this;
}

PSOBuilder& PSOBuilder::SetShaders(const std::wstring& vsPath, const std::wstring& psPath) {
	vsPath_ = vsPath; psPath_ = psPath;
	return *this;
}

PSOBuilder& PSOBuilder::SetComputeShader(const std::wstring& csPath) {
	csPath_ = csPath;
	return *this;
}

PSOBuilder& PSOBuilder::SetInputLayout(const D3D12_INPUT_ELEMENT_DESC* pElements, UINT numElements) {
	//inputElements_.assign(pElements, pElements + numElements);
	pInputElements_ = pElements;
	numInputElements_ = numElements;
	return *this;
}

PSOBuilder& PSOBuilder::SetBlendMode(BLENDMODE mode) {
	blendMode_ = mode;
	return *this;
}

PSOBuilder& PSOBuilder::SetTopologyType(const D3D12_PRIMITIVE_TOPOLOGY_TYPE& type) {
	topologyType_ = type;
	return *this;
}

PSOBuilder& PSOBuilder::SetRasterizerSettings(const RasterizerSettings& settings) {
	rasterizerSettings_ = settings;
	return *this;
}

PSOBuilder& PSOBuilder::SetDepthSettings(const DepthSettings& settings) {
	depthSettings_ = settings;
	return *this;
}

PSOBuilder& PSOBuilder::SetRenderTargetSettings(const RenderTargetSettings& settings) {
	renderTargetSettings_ = settings;
	return *this;
}

void PSOBuilder::Build() {
	// RootSignatureをビルド
	auto rootSignature = rsBuilder_.Build(fngine_->GetD3D12System().GetDevice().Get());

	PSO newPSO;

	if (pipelineType_ == "Graphics") {
		newPSO.InitializeDirectly(
			fngine_, 
			rootSignature, 
			pInputElements_, 
			numInputElements_, 
			blendMode_, 
			vsPath_, psPath_, 
			topologyType_, 
			rasterizerSettings_, 
			depthSettings_,
			renderTargetSettings_
		);
	}
	else if (pipelineType_ == "Compute") {
		newPSO.InitializeDirectlyCompute(fngine_, rootSignature, pInputElements_, numInputElements_, csPath_);
	}

	// マネージャーのマップに登録
	PipelineStateObjectManager::GetInstance()->RegisterPSO(name_, std::move(newPSO));
}