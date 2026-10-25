#include "IHair.h"
#include <d3dx12.h>
#include "CameraSystem.h"
#include "Chronos.h"
#include "SDFManager.h"
#include "StringPool.h"
#include "../Engine/Objects/Primitive/Box/PrimitiveBox.h"
#include "FileSystem.h"
#include <pix3.h>

// CPPのみで定義されている関数

// 各種サイズ定義（DX12の制約に合わせるアラインメント計算）
constexpr UINT AlignTo(UINT size, UINT alignment) {
	return (size + alignment - 1) & ~(alignment - 1);
}

void CreateShaderBindingTable(
	ID3D12StateObject* rtpso,
	ID3D12Device* device,
	Microsoft::WRL::ComPtr<ID3D12Resource>& sbtBuffer,
	D3D12_DISPATCH_RAYS_DESC& dispatchDesc) // 後で描画に使う設定をここに書き込む
{

	// -----------------------------------------------

		// PSOから「シェーダー識別子を取り出すためのインターフェース」をクエリする
	Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> rtpsoProps;
	rtpso->QueryInterface(IID_PPV_ARGS(&rtpsoProps));

	// 各シェーダーの識別子（一辺32バイトの固有データ）を取得
	void* rayGenId = rtpsoProps->GetShaderIdentifier(L"MyRayGenShader");    // あなたのRayGen名
	void* missId = rtpsoProps->GetShaderIdentifier(L"MyMissShader");      // あなたのMiss名
	void* hairHitId = rtpsoProps->GetShaderIdentifier(L"HairHitGroup");      // さっき登録した髪のグループ名

	// -----------------------------------------------

		// GPUが分かりやすいようにメモリを分けて配置。
		// サイズの計算（32バイトアラインメントを適用）
	UINT shaderIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES; // 常に32バイト

	UINT rayGenRecordSize = AlignTo(shaderIdSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT); // 32
	UINT missRecordSize = AlignTo(shaderIdSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT); // 32
	UINT hitGroupRecordSize = AlignTo(shaderIdSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT); // 32

	// 各テーブルの開始位置（オフセット）を計算（64バイトアラインメントを適用）
	UINT rayGenSectionSize = AlignTo(rayGenRecordSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
	UINT missSectionSize = AlignTo(missRecordSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
	UINT hitGroupSectionSize = AlignTo(hitGroupRecordSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

	UINT totalSBTSize = rayGenSectionSize + missSectionSize + hitGroupSectionSize;

	// -----------------------------------------------

		// 3. バッファの作成（Uploadヒープで作成）
	sbtBuffer = CreateBufferResource(device, totalSBTSize);

	// 4. メモリにシェーダー識別子を書き込む
	uint8_t* pData = nullptr;
	sbtBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pData));

	// 内部を一旦ゼロクリア（dataが残っている可能性をゼロにするため）
	std::memset(pData, 0, totalSBTSize);

	// 各セクションのポインタを計算
	uint8_t* pRayGenHeader = pData;
	uint8_t* pMissHeader = pRayGenHeader + rayGenSectionSize;
	uint8_t* pHitGroupHeader = pMissHeader + missSectionSize;

	// 識別子をコピー（memcpy）
	std::memcpy(pRayGenHeader, rayGenId, shaderIdSize);
	std::memcpy(pMissHeader, missId, shaderIdSize);
	std::memcpy(pHitGroupHeader, hairHitId, shaderIdSize);

	sbtBuffer->Unmap(0, nullptr);

	// -----------------------------------------------

		// 5. 【超重要】描画命令（DispatchRays）に渡すための設定（Desc）を埋める
	D3D12_GPU_VIRTUAL_ADDRESS sbtAddress = sbtBuffer->GetGPUVirtualAddress();

	// RayGenの設定(一描画につき一回しか使わないためTableでなくていい)
	dispatchDesc.RayGenerationShaderRecord.StartAddress = sbtAddress;
	dispatchDesc.RayGenerationShaderRecord.SizeInBytes = rayGenRecordSize;

	// Missの設定
	dispatchDesc.MissShaderTable.StartAddress = sbtAddress + rayGenSectionSize;
	dispatchDesc.MissShaderTable.SizeInBytes = missRecordSize;
	dispatchDesc.MissShaderTable.StrideInBytes = missRecordSize;

	// HitGroupの設定（ここに髪の毛の起動ボタンの情報が入る）
	dispatchDesc.HitGroupTable.StartAddress = sbtAddress + rayGenSectionSize + missSectionSize;
	dispatchDesc.HitGroupTable.SizeInBytes = hitGroupRecordSize;
	dispatchDesc.HitGroupTable.StrideInBytes = hitGroupRecordSize;
}

Microsoft::WRL::ComPtr<IDxcBlob> CompileRaytracingShader(
	const std::wstring& filePath,
	const wchar_t* profile, // ここには L"lib_6_5" などを渡します
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler)
{
	Log::View(ConvertString(std::format(L"Begin DXR CompileShader, path:{}, profile:{}\n", filePath, profile)));

	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	assert(SUCCEEDED(hr));

	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;

	// 【注目！】レイトレ（ライブラリ）用のコンパイル引数
	std::vector<LPCWSTR> arguments = {
		filePath.c_str(),
		L"-T", profile,          // L"lib_6_5" などを指定
		L"-HV", L"2021",         // float32_t 等を使うためにHLSL 2021規格を有効化
		L"-Zi", L"-Qembed_debug",// デバッグ用
		L"-Od",                  // 最適化オフ（デバッグしやすくする）
		L"-Zpr",                 // 行優先（Row Major）
	};

	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,
		arguments.data(),
		static_cast<UINT32>(arguments.size()),
		includeHandler,
		IID_PPV_ARGS(&shaderResult)
	);
	assert(SUCCEEDED(hr));

	// エラーチェック
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Log::View(shaderError->GetStringPointer());
		assert(false); // 警告・エラーがあればここで止まる
	}

	// 実行用バイナリの取得
	Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));

	Log::View(ConvertString(std::format(L"DXR CompileSucceeded, path:{}\n", filePath)));

	shaderSource->Release();
	shaderResult->Release();

	return shaderBlob;
}

Microsoft::WRL::ComPtr<ID3D12StateObject> Hair::CreateHairRaytracingPSO(
	ID3D12Device5* device, // DXR対応のDevice5が必要
	ID3D12RootSignature* globalRootSignature,
	const void* shaderByteCode,
	SIZE_T shaderByteCodeSize)
{
	// -----------------------------------------------

		// お助けクラス（Descヘルパー）を初期化
	CD3DX12_STATE_OBJECT_DESC pipelineDesc(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

	// -----------------------------------------------

		// DXILライブラリ（コンパイル済みシェーダー）の登録
	auto libSubobject = pipelineDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE(shaderByteCode, shaderByteCodeSize);
	libSubobject->SetDXILLibrary(&libdxil);

	// 使用するシェーダー関数名をDXRに登録 -> ShaderImportで登録
	libSubobject->DefineExport(L"MyRayGenShader");
	libSubobject->DefineExport(L"MyMissShader");
	libSubobject->DefineExport(L"HairIntersectionShader");
	libSubobject->DefineExport(L"HairClosestHitShader");

	// -----------------------------------------------

		// 【最重要】ヒットグループの作成
	auto hitGroupSubobject = pipelineDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	// 髪の毛は三角形ではないので「PROCEDURAL_PRIMITIVE」を指定します
	hitGroupSubobject->SetHitGroupType(D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE);
	// このヒットグループの名前を設定
	hitGroupSubobject->SetHitGroupExport(L"HairHitGroup");

	// 交差シェーダーとヒットシェーダーをこのグループに紐付ける
	hitGroupSubobject->SetIntersectionShaderImport(L"HairIntersectionShader");
	hitGroupSubobject->SetClosestHitShaderImport(L"HairClosestHitShader");

	// -----------------------------------------------

		// シェーダー間を行き来するデータ量の設定（データサイズの申告）
	auto shaderConfigSubobject = pipelineDesc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();

	// ペイロードサイズ: RayPayload（float3 color = 12バイト）
	UINT payloadSize = sizeof(float) * 3;
	// アトリビュートサイズ: Intersectionから渡される float2 attr（8バイト）
	UINT attributeSize = sizeof(float) * 5;
	shaderConfigSubobject->Config(payloadSize, attributeSize);

	// -----------------------------------------------

		// グローバルルートシグネチャの紐付け
	auto globalRootSigSubobject = pipelineDesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	globalRootSigSubobject->SetRootSignature(globalRootSignature);

	// -----------------------------------------------

		// パイプライン設定（レイの再帰の深さ）
	auto pipelineConfigSubobject = pipelineDesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	// 一次レイ（カメラからの光線）だけなら「1」でOK。鏡や影を作るなら増やします
	pipelineConfigSubobject->Config(1);

	// -----------------------------------------------

		// --- ついにビルド！ ---
	Microsoft::WRL::ComPtr<ID3D12StateObject> rtpso;
	device->CreateStateObject(pipelineDesc, IID_PPV_ARGS(&rtpso));

	return rtpso;
}

// -----------------------------------------------

void Hair::Initialize(Fngine* engine, bool isLoad, const Strands::HairSaveData& savedata) {
	//   ====================
	// 【 初期化時の共通処理 】
	//   ====================
	// ■engineの情報を取得
	engine_ = engine;
	auto commandList = engine->GetCommand().GetList().GetList();
	// ■【 RayTracingを使用するために4にキャストしたコマンドリストを取得 】
	commandList.As(&dxrCmdList_);

#ifdef USE_IMGUI
	// PIXイベントを開始
	PIXScopedEvent(dxrCmdList_.Get(), PIX_COLOR_INDEX(2), "Initialize Hair Pass");
#endif// USEIMGUI


	// ■出力先のTextureを作成
	outputTexture_ = std::make_unique<RWTexture2D>();
	outputTexture_->Initialize(engine, 1280, 720, DXGI_FORMAT_R8G8B8A8_UNORM);
	outputTexture_->GetResource()->SetName(L"HairOutputTexture");
	renderTargetUAVHandleGPU_ = outputTexture_->GetUAVHandleGPU();

	// -------------------
	// 【 髪のマテリアル 】
	// -------------------
	HairMaterialParameter materialParameter;
	gpuHairMaterialParameterBuffer_ = std::make_unique<ConstantBuffer<HairMaterialParameter>>(engine);
	gpuHairMaterialParameterBuffer_->Initialize();

	materialParameter.shift = 0.10f;
	materialParameter.roughness = 1.0f;
	materialParameter.specular = 1.0f;
	materialParameter.scatter = 1.0f;
	materialParameter.moveDirection = { 0.0f,0.0f,0.0f };
	materialParameter.mat = characterMatrix_;

	// -------------------
	// 【 髪のSceneデータ 】
	// -------------------
	HairSceneParameter sceneParameter;
	gpuHairSceneParameterBuffer_ = std::make_unique<ConstantBuffer<HairSceneParameter>>(engine);
	gpuHairSceneParameterBuffer_->Initialize();

	sceneParameter.gravity = { 0.0f,-0.98f,-0.0f };
	sceneParameter.windDirection = { -0.0f,0.0f,0.2f };
	sceneParameter.time = 0.0f;
	sceneParameter.deltaTime = 1.0f / 60.0f;
	std::memcpy(gpuHairSceneParameterBuffer_->GetMappedData(), &sceneParameter, sizeof(HairSceneParameter));

	gpuConfigBuffer_ = std::make_unique<ConstantBuffer<Strands::HairConfig>>(engine);
	gpuConfigBuffer_->Initialize();

#pragma region 消す予定のデータ
	// ------------------------
	// 【 髪への光に関する情報 】
	// ------------------------
	HairLightingParams lightingConfig;
	lightingConfig.lightDirection = { 0.51f, -0.5f, 0.66f };
	lightingConfig.lightColor = { 1.0f, 1.0f, 1.0f };
	lightingConfig.lightIntensity = 8.0f;
	lightingConfig.ambientColor = { 0.05f, 0.05f, 0.05f };
	lightingConfig.hairShift = 0.10f;     // 最初は少し根元寄りにシフト
	lightingConfig.hairExponent = 80.0f;  // 程よい細さの天使の輪
	hairLightingParamsBuffer_ = std::make_unique<ConstantBuffer<HairLightingParams>>(engine);
	hairLightingParamsBuffer_->Initialize();
	std::memcpy(hairLightingParamsBuffer_->GetMappedData(), &lightingConfig, sizeof(HairLightingParams));
#pragma endregion

	// -------------------
	//
	// 【　Bufferの初期化　】
	//
	// --------------------
	guideInfoBuffer_ = std::make_unique<Structured<GuideCurve::GuideInfo>>(engine);
	strandInfoBuffer_ = std::make_unique<Structured<Strands::StrandInfo>>(engine);

	// ガイドの総本数：Load - 取得　作成 - 作成
	uint32_t totalGuides = 0;
	UINT NUM_STRANDS = 0;
	UINT totalVertices = 0;
	Strands::HairConfig hairConfig;// Loadするもの

	if (isLoad == true) {
		// --------------------------------
		// 【 ガイドを参照するための情報 】
		// --------------------------------
		guideInfoBuffer_->Initialize(static_cast<uint32_t>(savedata.guideInfo.size()));
		std::memcpy(guideInfoBuffer_->GetMappedData(), &savedata.guideInfo, sizeof(GuideCurve::GuideInfo) * savedata.guideInfo.size());

		// ------------------
		// 【 ガイドを作成 】
		// ------------------
		totalGuides = savedata.hairConfig.numGuides;
		uint32_t controllerPointCount = static_cast<uint32_t>(savedata.points.size());

		// UAVの作成
		gpuGuideBuffer_ = std::make_unique<RWStructured<GuideCurve::ControllerPoint>>(engine);
		gpuGuideBuffer_->Initialize(controllerPointCount);

		uploadGuideBuffer = std::make_unique<Structured<GuideCurve::ControllerPoint>>(engine);
		uploadGuideBuffer->Initialize(controllerPointCount);
		std::memcpy(uploadGuideBuffer->GetMappedData(), savedata.points.data(), sizeof(GuideCurve::ControllerPoint) * controllerPointCount);

		strandInfoBuffer_->Initialize(static_cast<uint32_t>(savedata.strandInfos.size()));
		std::memcpy(strandInfoBuffer_->GetMappedData(), savedata.strandInfos.data(), sizeof(Strands::StrandInfo) * savedata.strandInfos.size());

		NUM_STRANDS = static_cast<UINT>(savedata.hairConfig.numStrands);
		gpuChildStrandBuffer_ = std::make_unique<Structured<Strands::ChildStrand>>(engine);
		gpuChildStrandBuffer_->Initialize(static_cast<UINT>(savedata.childStrands.size()));
		std::memcpy(gpuChildStrandBuffer_->GetMappedData(), savedata.childStrands.data(), sizeof(Strands::ChildStrand) * savedata.childStrands.size());
		Log::View("Child strands loaded from save data.");

		for (const auto& info : savedata.strandInfos) {
			totalVertices += static_cast<UINT>(info.vertexCount);
		}
	}	
	else {
		// --- 髪のMaterial ---
		materialParameter.stiffness = 50.4f;
		materialParameter.restoringForce = 50.2f;
		materialParameter.damping = 0.96f;
		materialParameter.globalSpreadRadius = 0.05f;
		materialParameter.globalHairThickness = 1.0f;
		std::memcpy(gpuHairMaterialParameterBuffer_->GetMappedData(), &materialParameter, sizeof(HairMaterialParameter));

		// 最大数
		hairConfig.numGuides = 1000;       // ガイドの総本数：1000
		hairConfig.pointPerGuide = 16;      // ガイド一本を作る最大数：16
		hairConfig.pointPerStrand = 32;     // ストランド一本を作る数：32
		hairConfig.numStrands = 50000.0f;     // ストランドの総本数：10000
		std::memcpy(gpuConfigBuffer_->GetMappedData(), &hairConfig, sizeof(Strands::HairConfig));

		totalGuides = hairConfig.numGuides;
		// ガイドを構成する頂点数の合計 = ガイドの本数 x ガイドを構成する頂点数
		UINT controllerPointSum = static_cast<UINT>(hairConfig.numGuides * hairConfig.pointPerGuide);

		guideInfoBuffer_->Initialize(hairConfig.numGuides);

		// UAVの作成：ガイドを管理するためのBuffer
		gpuGuideBuffer_ = std::make_unique<RWStructured<GuideCurve::ControllerPoint>>(engine);
		gpuGuideBuffer_->Initialize(controllerPointSum);

		// dataをコピーするためだけに作成
		uploadGuideBuffer = std::make_unique<Structured<GuideCurve::ControllerPoint>>(engine);
		uploadGuideBuffer->Initialize(controllerPointSum);

		strandInfoBuffer_->Initialize(static_cast<uint32_t>(hairConfig.numStrands * hairConfig.pointPerStrand));

		float spreadRadius = 0.005f;
		NUM_STRANDS = static_cast<UINT>(gpuConfigBuffer_->GetMappedData()->numStrands);
		std::vector<Strands::ChildStrand> childStrands;
		auto rand = RandomUtils::GetInstance();

		//   ================
		// 【 ストランド設計 】
		//   ================
		for (UINT s = 0; s < NUM_STRANDS; ++s) {
			Strands::ChildStrand child;

			uint32_t targetGuideId = rand->GetHighRandom().GetInt(0, gpuConfigBuffer_->GetMappedData()->numGuides - 1); // ランダムに1本選ぶ
			child.parentGuideIds[0] = targetGuideId;
			child.parentGuideIds[1] = targetGuideId;
			child.parentGuideIds[2] = targetGuideId;
			child.blendMode = 0; // 単一ガイド追従モード//

			child.weights[0] = 1.0f;
			child.weights[1] = 0.0f;
			child.weights[2] = 0.0f;

			child.twistAngle = Deg2Rad(1080.0f); // 3回転ドリルねじれ
			child.clumpForce = 0.8f;             // ギュッと束ねる

			//child.lengthScale = 0.9f + rand->GetHighRandom().GetFloat(0.0f, 0.2f);
			child.lengthScale = 1.0f;

			// ガイドの周りに散らすオフセット
			float angle = static_cast<float>(rand->GetHighRandom().GetInt(0, 360));
			float r = sqrtf(rand->GetHighRandom().GetFloat(0.0f, 1.0f)) * spreadRadius;
			child.offset = Vector2(std::cosf(Deg2Rad(angle)) * r, std::sinf(Deg2Rad(angle)) * r);

			child.seed = s; // チラつき防止固定シード
			child.waveAmplitude = 0.01f;
			child.waveFrequency = 4.0f;
			child.noise = 0.0f;

			childStrands.push_back(child);
		}

		gpuChildStrandBuffer_ = std::make_unique<Structured<Strands::ChildStrand>>(engine);
		gpuChildStrandBuffer_->Initialize(static_cast<UINT>(childStrands.size()));
		std::memcpy(gpuChildStrandBuffer_->GetMappedData(), childStrands.data(), sizeof(Strands::ChildStrand) * childStrands.size());

		Log::View("Child strands generated.\n");
		totalVertices = NUM_STRANDS * gpuConfigBuffer_->GetMappedData()->pointPerStrand;

		// もし、クラッシュしたりデータがゴミだった場合は、偽のデータを作成してとりあえず詰めてください
		Log::View("if you Clash when make only buffer ni Please nise data");
	}

	// 初期位置データを uploadGuideBuffer から gpuGuideBuffer_ へコピー
	auto transitionToCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(
		gpuGuideBuffer_->GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
	dxrCmdList_->ResourceBarrier(1, &transitionToCopyDest);

	dxrCmdList_->CopyResource(gpuGuideBuffer_->GetResource(), uploadGuideBuffer->GetResource());

	auto transitionToSRV = CD3DX12_RESOURCE_BARRIER::Transition(
		gpuGuideBuffer_->GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	dxrCmdList_->ResourceBarrier(1, &transitionToSRV);

	// ==========================================
	// 【 生成結果・AABB用バッファのメモリ確保 】
	// ==========================================
	hairVertexBuffer_ = std::make_unique<RWStructured<Strands::StrandVertex>>(engine);
	hairVertexBuffer_->Initialize(totalVertices);
	hairVertexBuffer_->GetResource()->SetName(L"HairVertexBuffer");

	//  ================================
	//  【　AABBBufferについての処理　】
	//  ================================
	std::vector<Strands::SegmentData> cpuSegments;
	if (isLoad == true) {
		cpuSegments = savedata.segments;
	}
	else {
		for (int i = 0; i < hairConfig.numStrands; ++i) {
			// 各髪の毛(ストランド)のセグメントを生成
			for (uint32_t j = 0; j < hairConfig.pointPerStrand - 1; ++j) {
				Strands::SegmentData seg;
				seg.v0_Index = i * hairConfig.pointPerStrand + j;
				seg.v1_Index = i * hairConfig.pointPerStrand + j + 1;
				cpuSegments.push_back(seg);
			}
		}
	}

	segmentBuffer_ = std::make_unique<Structured<Strands::SegmentData>>(engine);
	segmentBuffer_->Initialize(static_cast<uint32_t>(cpuSegments.size()));
	segmentBuffer_->GetResource()->SetName(L"HairSegmentBuffer");
	std::memcpy(segmentBuffer_->GetMappedData(), cpuSegments.data(), sizeof(Strands::SegmentData) * cpuSegments.size());

	hairFlatVertexBuffer_ = std::make_unique<RWStructured<Strands::StrandVertex>>(engine);
	hairFlatVertexBuffer_->Initialize(static_cast<uint32_t>(cpuSegments.size()) * 2);
	hairFlatVertexBuffer_->GetResource()->SetName(L"HairFlatVertexBuffer");

	hairAABBBuffer_ = std::make_unique<RWStructured<D3D12_RAYTRACING_AABB>>(engine);
	hairAABBBuffer_->Initialize(static_cast<uint32_t>(cpuSegments.size()));
	hairAABBBuffer_->GetResource()->SetName(L"HairAABBBuffer");

	Log::View("AABB Compleate\n");

	// =============================
	// 【 DXR用の加速構造体の作成 】
	// =============================
	auto pDevice = engine->GetD3D12System().GetDevice().Get();
	Microsoft::WRL::ComPtr <ID3D12Device5> pDevice5 = nullptr;
	HRESULT hr = pDevice->QueryInterface(IID_PPV_ARGS(&pDevice5));

	// --- BLASの構造体を作るための情報 ---
	// ジオメトリの設定
	D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc{};
	// ジオメトリの種類
	geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS; // AABBを指定
	// ジオメトリのフラグ
	geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
	// 【 AABBの数を設置場所 】
	geometryDesc.AABBs.AABBCount = hairAABBBuffer_->GetNumElements();
	geometryDesc.AABBs.AABBs.StartAddress = hairAABBBuffer_->GetResource()->GetGPUVirtualAddress();
	geometryDesc.AABBs.AABBs.StrideInBytes = sizeof(D3D12_RAYTRACING_AABB);

	//   ======================
	// 【 BLASを作るための情報 】
	//   ======================
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS buildInputs{};
	// Type : 構築する加速度構造体の種類
	buildInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL; // BLASを指定
	// Flags : フラグ
	buildInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE
		| D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE; // レイの追跡を高速化
	/*switch (Type) {
		case D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TOP_LEVEL
			DescsLayout に基づいてレイアウトされたインスタンスの数
		case D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BOTTOM_LEVEL
			pGeometryDescs または ppGeometryDescs によって参照される要素の数
	}*/
	buildInputs.NumDescs = 1;
	// BLASの構造体を作るための情報をセッティング
	buildInputs.pGeometryDescs = &geometryDesc;

	//   =====================================================
	// 【 GPUにBLASをビルドするのに必要サイズを計算してもらう 】
	//   =====================================================
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo{};

	if (SUCCEEDED(hr)) {
		// ビルドした後に必要なサイズ、ビルドするのにかかるサイズ、其のあとに調整するときにかかるサイズ
		pDevice5->GetRaytracingAccelerationStructurePrebuildInfo(&buildInputs, &prebuildInfo);
	}
	else {
		// そもそもこのパソコン（グラボ）がレイトレ非対応の可能性あり
	}

	// Rebuild用とUpdate用の「大きい方のサイズ」を取得する
	UINT64 maxScratchSize = (std::max)(prebuildInfo.ScratchDataSizeInBytes, prebuildInfo.UpdateScratchDataSizeInBytes);

	// 作業用のBuffer（最大サイズで確保）
	scratchBuffer_ = CreateBufferResource(pDevice, maxScratchSize, true);
	scratchBuffer_->SetName(L"BLAS Scratch Buffer");

	D3D12_HEAP_PROPERTIES heapProps{ D3D12_HEAP_TYPE_DEFAULT };
	D3D12_RESOURCE_DESC resDesc{};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = prebuildInfo.ResultDataMaxSizeInBytes; // 必要なサイズ
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.Format = DXGI_FORMAT_UNKNOWN;
	resDesc.SampleDesc.Count = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // UAVフラグ

	pDevice->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, // ★初期状態を最初からこれにする！
		nullptr,
		IID_PPV_ARGS(&blasResultBuffer_)
	);

	blasResultBuffer_->SetName(L"BLAS Result Buffer");

	// ==================================
	// 【 TLASの構造体を作るための情報 】
	// ==================================
	// --- 配置図（Instance Desc）を1個作る ---
	D3D12_RAYTRACING_INSTANCE_DESC instanceDesc{};

	// 3x4の行列で位置と向きを指定（今回は回転なし・原点配置の単位行列）
	instanceDesc.Transform[0][0] = 1.0f;
	instanceDesc.Transform[1][1] = 1.0f;
	instanceDesc.Transform[2][2] = 1.0f;
	// シェーダー側で識別するためのID
	instanceDesc.InstanceID = 0;
	// どのレイにヒットするかのマスク設定
	instanceDesc.InstanceMask = 0xFF;
	// ヒットグループのオフセット
	instanceDesc.InstanceContributionToHitGroupIndex = 0;
	// オブジェクトの設定
	instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
	// さっき作った「BLAS」のGPUアドレスをここに紐付ける！
	instanceDesc.AccelerationStructure = blasResultBuffer_->GetGPUVirtualAddress();

	// --- 君の Structured クラスにデータを載せる ---
	// ※今後、複数のキャラクターや髪の束を増やす時は要素数を増やせばOK
	instanceBuffer = std::make_unique<Structured<D3D12_RAYTRACING_INSTANCE_DESC>>(engine);
	instanceBuffer->Initialize(1);
	instanceBuffer->GetResource()->SetName(L"TLAS Instance Buffer");
	std::memcpy(instanceBuffer->GetMappedData(),&instanceDesc,sizeof(D3D12_RAYTRACING_INSTANCE_DESC));

	// ==========================
	// 【 TLASを作るための情報 】
	// ==========================
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs{};
	// TLASを指定
	tlasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	// 作り方の指定
	tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE | 
		               D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
	// BLASの総数
	tlasInputs.NumDescs = 1; // インスタンス（配置図）の数
	// dataの並び方
	tlasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	// BLASのGPUアドレス
	tlasInputs.InstanceDescs = instanceBuffer->GetResource()->GetGPUVirtualAddress();

	// =========================================================
	// 【 GPUにTLASをビルドするのに必要サイズを計算してもらう 】
	// =========================================================
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasPrebuildInfo{};
	pDevice5->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasPrebuildInfo);

	// TLAS用のスクラッチバッファ（作業場所の確保）
	tlasScratchBuffer_ = CreateBufferResource(pDevice, tlasPrebuildInfo.ScratchDataSizeInBytes, true);
	tlasScratchBuffer_->SetName(L"TLAS Scratch Buffer");

	resDesc.Width = tlasPrebuildInfo.ResultDataMaxSizeInBytes;
	pDevice->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
		nullptr,
		IID_PPV_ARGS(&tlasResultBuffer_)
	);
	tlasResultBuffer_->SetName(L"TLAS Result Buffer");

	// -------------------
	// 【 RootParameter 】
	// -------------------
	CD3DX12_ROOT_PARAMETER rootParameters[6]{};
	rootParameters[0].InitAsConstantBufferView(0);// b0: カメラ情報 (Constant Buffer)
	rootParameters[1].InitAsConstantBufferView(1);// b1: カメラ情報 (Constant Buffer)
	rootParameters[2].InitAsShaderResourceView(0);// t0: TLAS (Shader Resource View)
	rootParameters[3].InitAsShaderResourceView(1);// t1: 髪の毛の頂点バッファ (Shader Resource View)

	// u0: 出力先のキャンバス (Unordered Access View)
	CD3DX12_DESCRIPTOR_RANGE uavRange;
	uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // u0 を1個
	rootParameters[4].InitAsDescriptorTable(1, &uavRange);

	CD3DX12_DESCRIPTOR_RANGE srvRange;
	srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2); // t2 を1個
	rootParameters[5].InitAsDescriptorTable(1, &srvRange);

	// ---------------------------------------------------------
	// 配線図を1つにまとめる
	// ---------------------------------------------------------
	CD3DX12_ROOT_SIGNATURE_DESC globalRootSigDesc(ARRAYSIZE(rootParameters), rootParameters);

	// ---------------------------------------------------------
	// シリアライズしてGPU用のRootSignatureオブジェクトを作成
	// ---------------------------------------------------------
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	D3D12SerializeRootSignature(&globalRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);

	if (errorBlob) {
		// エラーが出たら中身を出力（OutputDebugStringなど）
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}

	// メンバ変数の globalRootSignature_ に格納！
	pDevice->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&globalRootSignature_));

	hairShaderBlob_ = CompileRaytracingShader(
		L"resources/shaders/Hair/HairRaytracing.hlsl",
		L"lib_6_5",
		engine->GetDXC().GetUtils().Get(),
		engine->GetDXC().GetCompiler().Get(),
		engine->GetDXC().GetIncludeHandle().Get()
	);

	rtpso_ = CreateHairRaytracingPSO(
		pDevice5.Get(),
		globalRootSignature_.Get(),
		hairShaderBlob_->GetBufferPointer(),
		hairShaderBlob_->GetBufferSize()
	);

	// シェーダーバインディングテーブルの作成
	CreateShaderBindingTable(
		rtpso_.Get(),
		pDevice,
		sbtBuffer_,
		dispatchDesc_
	);

	hairCameraBuffer_ = std::make_unique<ConstantBuffer<HairCamera>>(engine_);
	hairCameraBuffer_->Initialize();

	auto initBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
		outputTexture_->GetResource(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);
	dxrCmdList_->ResourceBarrier(1, &initBarrier);

	LoadFromFile("resources/Data/Hair/CompHair2.json");

	isGpuUpdateRequested_ = true;
}
//////////////////////////////
//   ====================
// 【      更新処理      】
//   ====================
//////////////////////////////
void Hair::Update(float deltaTime, const Matrix4x4& mat) {

	auto chronos = Chronos::GetInstance();
	characterMatrix_ = mat;

	// ===========================
	// 【 GPUへパラメータを統合 】
	// ===========================
	gpuHairMaterialParameterBuffer_->GetMappedData()->mat = Matrix4x4::Transpose(characterMatrix_);

	// 1/30秒（約0.033秒）以上に deltaTime が跳ね上がらないように制限する
	float safeDeltaTime = std::min(chronos->GetGameDeltaTime(), 1.0f / 30.0f);
	gpuHairSceneParameterBuffer_->GetMappedData()->deltaTime = safeDeltaTime;
	gpuHairSceneParameterBuffer_->GetMappedData()->time = chronos->GetGameTime();

	// mat から現在の位置を抽出
	Vector3 currentPos = { mat.m[3][0], mat.m[3][1], mat.m[3][2] };

	if (!isFirstFrame_) {
		// ワールド空間での移動量（今回 - 前回）
		Vector3 worldMove = currentPos - prevPosition_;
		gpuHairMaterialParameterBuffer_->GetMappedData()->moveDirection = worldMove;
	}
	else {
		// 座標変換
		if (uploadGuideBuffer) {
			auto* points = uploadGuideBuffer->GetMappedData();
			uint32_t count = uploadGuideBuffer->GetNumElements();
			for (uint32_t i = 0; i < count; ++i) {
				// homePosition(ローカル) を ワールド座標 に初回変換
				points[i].position = Matrix4x4::Transform(points[i].homePosition, mat);
				points[i].velocity = Vector3(0.0f, 0.0f, 0.0f);
			}
			RequestNotifyUpdate(); // GPUへの転送をリクエスト
		}
		isFirstFrame_ = false;
	}
	prevPosition_ = currentPos;

	ID3D12DescriptorHeap* ppHeaps[] = { SRVManager::GetInstance()->GetHeap() };
	// コマンドリストにヒープをセット（テーブルをセットするより【前】に呼ぶ！）
	dxrCmdList_->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

#ifdef USE_IMGUI
	// PIXイベントを開始
	PIXScopedEvent(dxrCmdList_.Get(), PIX_COLOR_INDEX(2), "Update Hair Pass");
#endif// USEIMGUI

	bool isRebuildBLAS = isGpuUpdateRequested_;

	// おそらくisGpuUpdateRequested_がTrueになるとPhysicsCSが動かなくなるバグがある。
	if (isGpuUpdateRequested_) {
		// コピー先に遷移
		auto transitionToCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(
			gpuGuideBuffer_->GetResource(),
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_COPY_DEST);

		// バリア開始
		D3D12_RESOURCE_BARRIER barriers[] = { transitionToCopyDest };
		dxrCmdList_->ResourceBarrier(1, barriers);

		// コピー開始
		dxrCmdList_->CopyResource(gpuGuideBuffer_->GetResource(), uploadGuideBuffer->GetResource());

		// コピーが終わったら、UAVバッファを「UAVとして使える状態」に戻す
		auto transitionToUAV = CD3DX12_RESOURCE_BARRIER::Transition(
			gpuGuideBuffer_->GetResource(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		dxrCmdList_->ResourceBarrier(1, &transitionToUAV);

		isGpuUpdateRequested_ = false;
	}

	//   ====================
	// 【 Compute Shader の リソースバリア処理 】
	//   ====================
	D3D12_RESOURCE_BARRIER preCSBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(gpuGuideBuffer_->GetResource(),
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		CD3DX12_RESOURCE_BARRIER::Transition(hairVertexBuffer_->GetResource(),
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		CD3DX12_RESOURCE_BARRIER::Transition(hairAABBBuffer_->GetResource(),
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		CD3DX12_RESOURCE_BARRIER::Transition(hairFlatVertexBuffer_->GetResource(),
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	};
	dxrCmdList_->ResourceBarrier(_countof(preCSBarriers), preCSBarriers);

	//   =====================
	// 【 HairGuideの物理演算 】
	//   =====================
	uint32_t totalGuides = gpuConfigBuffer_->GetMappedData()->numGuides;
	uint32_t physicsGroupX = (totalGuides + 63) / 64;

	dxrCmdList_->SetComputeRootSignature(PSOManager::GetInstance()->GetPSO("HairPhysicsCS").GetRootSignature().GetRS().Get());
	dxrCmdList_->SetPipelineState(PSOManager::GetInstance()->GetPSO("HairPhysicsCS").GetCPS().Get());

	dxrCmdList_->SetComputeRootConstantBufferView(0, gpuHairMaterialParameterBuffer_->GetGPUVirtualAddress());
	dxrCmdList_->SetComputeRootConstantBufferView(1, gpuHairSceneParameterBuffer_->GetGPUVirtualAddress());
	dxrCmdList_->SetComputeRootConstantBufferView(2, gpuConfigBuffer_->GetGPUVirtualAddress());
	uint32_t id = StringPool::GetInstance()->GetStringID("Naira_ExportTest");
	dxrCmdList_->SetComputeRootConstantBufferView(3, SDFManager::GetInstance()->tempBakeResources_[id].configBuffer->GetGPUVirtualAddress());
	dxrCmdList_->SetComputeRootDescriptorTable(4, guideInfoBuffer_->GetSRVHandleGPU());
	dxrCmdList_->SetComputeRootDescriptorTable(5, SDFManager::GetInstance()->GetSDF("Naira_ExportTest")->texture.GetSRVHandleGPU());
	dxrCmdList_->SetComputeRootDescriptorTable(6, gpuGuideBuffer_->GetUAVHandleGPU());

	dxrCmdList_->Dispatch(physicsGroupX, 1, 1);

	auto barrierGuide = CD3DX12_RESOURCE_BARRIER::Transition(
		gpuGuideBuffer_->GetResource(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	dxrCmdList_->ResourceBarrier(1, &barrierGuide);

	//   =====================
	// 【 Strands作成の再計算 】
	//   =====================
	uint32_t totalStrands = gpuChildStrandBuffer_->GetNumElements();
	uint32_t generateGroupX = (totalStrands + 63) / 64;

	dxrCmdList_->SetComputeRootSignature(PSOManager::GetInstance()->GetPSO("HairGenerateCS").GetRootSignature().GetRS().Get());
	dxrCmdList_->SetPipelineState(PSOManager::GetInstance()->GetPSO("HairGenerateCS").GetCPS().Get());

	dxrCmdList_->SetComputeRootDescriptorTable(0, gpuGuideBuffer_->GetSRVHandleGPU());
	dxrCmdList_->SetComputeRootDescriptorTable(1, gpuChildStrandBuffer_->GetSRVHandleGPU());
	dxrCmdList_->SetComputeRootDescriptorTable(2, strandInfoBuffer_->GetSRVHandleGPU());
	dxrCmdList_->SetComputeRootDescriptorTable(3, guideInfoBuffer_->GetSRVHandleGPU());
	dxrCmdList_->SetComputeRootDescriptorTable(4, hairVertexBuffer_->GetUAVHandleGPU());
	dxrCmdList_->SetComputeRootConstantBufferView(5, gpuHairMaterialParameterBuffer_->GetGPUVirtualAddress());
	dxrCmdList_->SetComputeRootConstantBufferView(6, gpuConfigBuffer_->GetGPUVirtualAddress());

	dxrCmdList_->Dispatch(generateGroupX, 1, 1);

	auto barrierVertices = CD3DX12_RESOURCE_BARRIER::Transition(
		hairVertexBuffer_->GetResource(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	dxrCmdList_->ResourceBarrier(1, &barrierVertices);

	//   ==================
	// 【 AABB作成の再計算 】
	//   ==================
	dxrCmdList_->SetComputeRootSignature(PSOManager::GetInstance()->GetPSO("HairAABBCS").GetRootSignature().GetRS().Get());
	dxrCmdList_->SetPipelineState(PSOManager::GetInstance()->GetPSO("HairAABBCS").GetCPS().Get());
	// ここにRootSignatureのやつを設定していく
	dxrCmdList_->SetComputeRootConstantBufferView(0, gpuConfigBuffer_->GetGPUVirtualAddress());
	dxrCmdList_->SetComputeRootDescriptorTable(1, hairVertexBuffer_->GetSRVHandleGPU());
	dxrCmdList_->SetComputeRootDescriptorTable(2, strandInfoBuffer_->GetSRVHandleGPU());
	dxrCmdList_->SetComputeRootDescriptorTable(3, hairAABBBuffer_->GetUAVHandleGPU());
	dxrCmdList_->SetComputeRootDescriptorTable(4, hairFlatVertexBuffer_->GetUAVHandleGPU());

	dxrCmdList_->Dispatch(generateGroupX, 1, 1);

	D3D12_RESOURCE_BARRIER postAABBBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(hairAABBBuffer_->GetResource(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
		CD3DX12_RESOURCE_BARRIER::Transition(hairFlatVertexBuffer_->GetResource(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
	};
	dxrCmdList_->ResourceBarrier(_countof(postAABBBarriers), postAABBBarriers);
	
	//   ==============
	// 【 BLASの再構築 】
	//   ==============
	// ジオメトリ（AABBバッファ）の設定を最新の状態に更新
	D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc{};
	geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
	geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
	geometryDesc.AABBs.AABBCount = hairAABBBuffer_->GetNumElements();
	geometryDesc.AABBs.AABBs.StartAddress = hairAABBBuffer_->GetResource()->GetGPUVirtualAddress();
	geometryDesc.AABBs.AABBs.StrideInBytes = sizeof(D3D12_RAYTRACING_AABB);

	// ビルド用のインプットを再設定
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs{};
	blasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	blasInputs.NumDescs = 1;
	blasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	blasInputs.pGeometryDescs = &geometryDesc;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasBuildDesc{};
	// アップデートなので、結果を既存のBLASバッファに上書きする
	blasBuildDesc.DestAccelerationStructureData = blasResultBuffer_->GetGPUVirtualAddress();
	blasBuildDesc.ScratchAccelerationStructureData = scratchBuffer_->GetGPUVirtualAddress();
	if (isRebuildBLAS) {
		// 【 FULL REBUILD 】ツリー構造をゼロから再構築
		// 次回の軽量更新を許可するために ALLOW_UPDATE を付与
		blasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE
			| D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;

		// 新規作成時は Source に 0 を指定
		blasBuildDesc.SourceAccelerationStructureData = 0;
	}
	else {
		// 【 UPDATE 】既存の BVH 構造を維持したまま AABB 位置のみを更新
		blasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE
			| D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE
			| D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE; // ← Updateフラグ

		// 更新元（Source）として現在の BLAS アドレスを指定
		blasBuildDesc.SourceAccelerationStructureData = blasResultBuffer_->GetGPUVirtualAddress();
	}
	blasBuildDesc.Inputs = blasInputs;

	// GPUに向けて、BLASのアップデートコマンドを発行！
	dxrCmdList_->BuildRaytracingAccelerationStructure(&blasBuildDesc, 0, nullptr);

	// BLASの書き込み完了を待つUAVバリア
	D3D12_RESOURCE_BARRIER blasBarrier{};
	blasBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	blasBarrier.UAV.pResource = blasResultBuffer_.Get();
	dxrCmdList_->ResourceBarrier(1, &blasBarrier);

	// -----------------------------------------------
		//   ================
		// 【 TLASの更新処理 】
		//   ================
	// -----------------------------------------------

	Matrix4x4 identity = Matrix4x4::Make::Identity();

	instanceBuffer->GetMappedData()->Transform[0][0] = identity.m[0][0];
	instanceBuffer->GetMappedData()->Transform[0][1] = identity.m[1][0];
	instanceBuffer->GetMappedData()->Transform[0][2] = identity.m[2][0];
	instanceBuffer->GetMappedData()->Transform[0][3] = identity.m[3][0];
	instanceBuffer->GetMappedData()->Transform[1][0] = identity.m[0][1];
	instanceBuffer->GetMappedData()->Transform[1][1] = identity.m[1][1];
	instanceBuffer->GetMappedData()->Transform[1][2] = identity.m[2][1];
	instanceBuffer->GetMappedData()->Transform[1][3] = identity.m[3][1];
	instanceBuffer->GetMappedData()->Transform[2][0] = identity.m[0][2];
	instanceBuffer->GetMappedData()->Transform[2][1] = identity.m[1][2];
	instanceBuffer->GetMappedData()->Transform[2][2] = identity.m[2][2];
	instanceBuffer->GetMappedData()->Transform[2][3] = identity.m[3][2];

	// -----------------------------------------------

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs{};
	tlasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	tlasInputs.NumDescs = 1;
	tlasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	tlasInputs.InstanceDescs = instanceBuffer->GetResource()->GetGPUVirtualAddress();

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasBuildDesc{};
	tlasBuildDesc.Inputs = tlasInputs;
	tlasBuildDesc.ScratchAccelerationStructureData = tlasScratchBuffer_->GetGPUVirtualAddress();
	tlasBuildDesc.DestAccelerationStructureData = tlasResultBuffer_->GetGPUVirtualAddress();

	if (isRebuildBLAS) {
		// 【 FULL REBUILD 】初回またはリクエスト時はゼロから新規構築
		tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE
			| D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
		tlasBuildDesc.SourceAccelerationStructureData = 0; // ソースなし (新規)
	}
	else {
		// 【 UPDATE 】2フレーム目以降の通常フレームは差分更新
		tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE
			| D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE
			| D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
		tlasBuildDesc.SourceAccelerationStructureData = tlasResultBuffer_->GetGPUVirtualAddress();
	}
	tlasBuildDesc.Inputs = tlasInputs; // 設定したFlagsを反映

	// GPUにビルドし直せコマンドを積む
	dxrCmdList_->BuildRaytracingAccelerationStructure(&tlasBuildDesc, 0, nullptr);

	// TLASのビルドが終わるまで、この先のレイ飛ばし（DispatchRays）を待たせるバリア
	D3D12_RESOURCE_BARRIER tlasBarrier{};
	tlasBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	tlasBarrier.UAV.pResource = tlasResultBuffer_.Get();
	dxrCmdList_->ResourceBarrier(1, &tlasBarrier);

	// -----------------------------------------------
#ifdef USE_IMGUI
	ImGui::Begin("Hair");
	ImGui::DragFloat3("gravity", &gpuHairSceneParameterBuffer_->GetMappedData()->gravity.x, 0.01f, -1.0f, 1.0f);
	ImGui::DragFloat3("Wind Direction ", &gpuHairSceneParameterBuffer_->GetMappedData()->windDirection.x, 0.01f, -1.0f, 1.0f);
	ImGui::DragFloat("globalHairThickness ", &gpuHairMaterialParameterBuffer_->GetMappedData()->globalHairThickness, 0.0001f, 0.0f, 1.0f);
	ImGui::DragFloat("globalSpreadRadius ", &gpuHairMaterialParameterBuffer_->GetMappedData()->globalSpreadRadius, 0.0001f, 0.0f, 1.0f);
	ImGui::DragFloat("stiffness：剛性 ", &gpuHairMaterialParameterBuffer_->GetMappedData()->stiffness, 0.0001f, 0.0f, 1.0f);
	ImGui::DragFloat("restoringForce：復元力", &gpuHairMaterialParameterBuffer_->GetMappedData()->restoringForce, 0.0001f, 0.0f, 1.0f);
	ImGui::DragFloat("damping：減衰力", &gpuHairMaterialParameterBuffer_->GetMappedData()->damping, 0.0001f, 0.0f, 1.0f);

	// --- ライティング・見た目の設定 ---
	ImGui::Separator(); // 境界線
	ImGui::Text("【 ライティング ＆ ツヤ（天使の輪）設定 】");
	// ライト方向
	ImGui::DragFloat3("ライトの照射方向", &hairLightingParamsBuffer_->GetMappedData()->lightDirection.x, 0.01f, -1.0f, 1.0f);
	// ライトカラー
	ImGui::ColorEdit3("ライトの色", &hairLightingParamsBuffer_->GetMappedData()->lightColor.x);
	// ライト強度
	ImGui::DragFloat("ライトの明るさ", &hairLightingParamsBuffer_->GetMappedData()->lightIntensity, 0.05f, 0.0f, 10.0f);
	// 環境光
	ImGui::ColorEdit3("暗部の明るさ (環境光)", &hairLightingParamsBuffer_->GetMappedData()->ambientColor.x);
	// 天使の輪のズレ（シフト）
	// 左右にスライドすると、天使の輪が毛先側・根元側にリアルタイムで上下移動します
	ImGui::DragFloat("天使の輪の位置 (シフト)", &gpuHairMaterialParameterBuffer_->GetMappedData()->shift, 0.005f, -0.5f, 0.5f);
	// 天使の輪のシャープさ
	// 小さいとボヤッと全体が光り、大きくするとキュッと細い綺麗な輪になります
	ImGui::DragFloat("天使の輪の鋭さ (ツヤ)", &hairLightingParamsBuffer_->GetMappedData()->hairExponent, 0.5f, 1.0f, 500.0f);
	ImGui::End();
#endif// USE_IMGUI
}

void Hair::Render(D3D12_GPU_DESCRIPTOR_HANDLE depthHandle) {
	// Get Camera Data
	HairCamera cameraData;
	cameraData.inverseProjView = Matrix4x4::Inverse(CameraSystem::GetInstance()->GetActiveCamera()->GetViewProjectionMatrix());
	cameraData.cameraPosition = CameraSystem::GetInstance()->GetActiveCamera()->GetTranslation();
	cameraData.padding = 0.0f; // パディングを0で初期化
	hairCameraBuffer_->GetMappedData()[0] = cameraData;

	auto commandList = engine_->GetCommand().GetList().GetList().Get();
#ifdef USE_IMGUI
	// PIXイベントを開始
	PIXScopedEvent(commandList, PIX_COLOR_INDEX(2), "Render Hair Pass");
#endif// USEIMGUI

	// --- グローバルルートシグネチャの設定 ---
	// レイトレ全体で使う共通のバッファ（TLASや出力テクスチャなど）をシェーダーに紐付け
	commandList->SetComputeRootSignature(globalRootSignature_.Get());

	commandList->SetComputeRootConstantBufferView(0, hairCameraBuffer_->GetGPUVirtualAddress());

	commandList->SetComputeRootConstantBufferView(1, hairLightingParamsBuffer_->GetGPUVirtualAddress());

	commandList->SetComputeRootShaderResourceView(2, tlasResultBuffer_->GetGPUVirtualAddress());
	// hairVertex->flat
	commandList->SetComputeRootShaderResourceView(3, hairFlatVertexBuffer_->GetResource()->GetGPUVirtualAddress());

	//commandList->SetComputeRootShaderResourceView(3, segmentBuffer_->GetResource()->GetGPUVirtualAddress());

	commandList->SetComputeRootDescriptorTable(4, renderTargetUAVHandleGPU_);

	commandList->SetComputeRootDescriptorTable(5, depthHandle);

	// --- パイプライン（RTPSO）のセット ---
	// コマンドリストにレイトレ専用パイプラインを設定（StateObjectをセットします）
	dxrCmdList_->SetPipelineState1(rtpso_.Get());

	// --- DXRレイトレーシングの実行！ ---
	// 前ステップで SBT の情報を詰め込んだ `dispatchDesc` に、画面サイズを設定します
	UINT windowWidth = 1280;  // 画面の横幅（例: 1920）
	UINT windowHeight = 720; // 画面の縦幅（例: 1080）
	dispatchDesc_.Width = windowWidth;  // 画面の横幅（例: 1920）
	dispatchDesc_.Height = windowHeight; // 画面の縦幅（例: 1080）
	dispatchDesc_.Depth = 1;            // 2D画面なので1でOK

	// GPUにレイトレーシングの開始を命令！
	dxrCmdList_->DispatchRays(&dispatchDesc_);
}

//////////////////////////////////////
///
///  ココから下にあるやつは描画したあとの処理
/// 
//////////////////////////////////////

void Hair::PreHair(ID3D12Resource* randerTargetResource, D3D12_RESOURCE_STATES preState, ID3D12Resource* depthResource) {
	auto commandList = engine_->GetCommand().GetList().GetList().Get();

#ifdef USE_IMGUI
	// PIXイベントを開始
	PIXScopedEvent(commandList, PIX_COLOR_INDEX(2), "Pre Hair Pass");
#endif// USEIMGUI

	// 今描画されている画像をコピーする
	auto transitionTargetSrc = CD3DX12_RESOURCE_BARRIER::Transition(
		randerTargetResource,
		preState,
		D3D12_RESOURCE_STATE_COPY_SOURCE);
	// コピー先
	auto transitionUAVDest = CD3DX12_RESOURCE_BARRIER::Transition(
		outputTexture_->GetResource(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COPY_DEST);

	auto transitionDepth = CD3DX12_RESOURCE_BARRIER::Transition(
		depthResource,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,                  // 元の状態
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE      // レイトレ(Compute)で読める状態
	);

	D3D12_RESOURCE_BARRIER barriers1[] = { transitionTargetSrc, transitionUAVDest, transitionDepth };
	commandList->ResourceBarrier(_countof(barriers1), barriers1);

	// それまでに描画されていた絵（青色など）をUAVテクスチャへ丸ごとコピー！
	commandList->CopyResource(outputTexture_->GetResource(), randerTargetResource);

	// -------------------------------------------------------

		// UAV（レイトレ結果）を「書き込み用(UAV)」から「コピー元(COPY_SOURCE)」へ変更
	auto transitionToCopySrc = CD3DX12_RESOURCE_BARRIER::Transition(
		outputTexture_->GetResource(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	// バックバッファ（画面）を「表示用(PRESENT)」などから「コピー先(COPY_DEST)」へ変更
	auto transitionToCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(
		randerTargetResource,
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_COPY_DEST);

	D3D12_RESOURCE_BARRIER barriers[] = { transitionToCopySrc, transitionToCopyDest };
	commandList->ResourceBarrier(_countof(barriers), barriers);
}

void Hair::PostHair(ID3D12Resource* randerTargetResource, D3D12_RESOURCE_STATES postState, ID3D12Resource* depthResource) {
	auto commandList = engine_->GetCommand().GetList().GetList().Get();

#ifdef USE_IMGUI
	// PIXイベントを開始
	PIXScopedEvent(commandList, PIX_COLOR_INDEX(2), "Post Hair Pass");
#endif// USEIMGUI

	auto transitionToCopySrc = CD3DX12_RESOURCE_BARRIER::Transition(
		outputTexture_->GetResource(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(1, &transitionToCopySrc);

	// UAVの絵を、バックバッファに全コピー！
	commandList->CopyResource(randerTargetResource, outputTexture_->GetResource());

	// バックバッファを「コピー先」から「表示用(PRESENT)」に戻す
	auto transitionToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
		randerTargetResource,
		D3D12_RESOURCE_STATE_COPY_DEST,
		postState);
	commandList->ResourceBarrier(1, &transitionToPresent);

	auto transitionToUAV = CD3DX12_RESOURCE_BARRIER::Transition(
		outputTexture_->GetResource(),
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	commandList->ResourceBarrier(1, &transitionToUAV);

	auto depthTransition = CD3DX12_RESOURCE_BARRIER::Transition(
		depthResource,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_DEPTH_WRITE
	);
	commandList->ResourceBarrier(1, &depthTransition);
}

bool Hair::SaveToFile(const std::string& filename) {
	auto* hairConfig = GetCPUGuideConfig();
	if (!hairConfig || !guideInfoBuffer_ || !strandInfoBuffer_) return false;

	Strands::HairSaveData saveData{};

	// コンフィグ類のコピー
	saveData.hairConfig = *hairConfig;
	//if (auto* physics = GetCPUPhysicsConfig()) { saveData.physicsConfig = *physics; }
	//if (auto* make = GetCPUMakeConfig()) { saveData.makeConfig = *make; }

	// --- データ収集ロジック ---
	uint32_t activeGuidePoints = 0;
	for (uint32_t i = 0; i < guideInfoBuffer_->GetNumElements(); ++i) {
		if (guideInfoBuffer_->GetMappedData()[i].vertexCount > 0) {
			activeGuidePoints += guideInfoBuffer_->GetMappedData()[i].vertexCount;
		}
	}
	if (activeGuidePoints > 0) {
		auto* data = GetCPUGuideData();
		saveData.points.assign(data, data + activeGuidePoints);
	}

	uint32_t activeGuides = 0;
	for (uint32_t i = 0; i < guideInfoBuffer_->GetNumElements(); ++i) {
		if (guideInfoBuffer_->GetMappedData()[i].vertexCount > 0) { ++activeGuides; }
	}
	if (activeGuides > 0) {
		auto* data = GetCPUGuideInfoData();
		saveData.guideInfo.assign(data, data + activeGuides);
	}

	uint32_t activeStrands = 0;
	for (uint32_t i = 0; i < strandInfoBuffer_->GetNumElements(); ++i) {
		if (strandInfoBuffer_->GetMappedData()[i].vertexCount > 0) { ++activeStrands; }
	}
	if (activeStrands > 0) {
		auto* infoData = GetCPUStrandInfoData();
		auto* childData = GetCPUChildStrandData();
		saveData.strandInfos.assign(infoData, infoData + activeStrands);
		saveData.childStrands.assign(childData, childData + activeStrands);
	}

	uint32_t totalSegments = 0;
	for (const auto& info : saveData.strandInfos) {
		if (info.vertexCount > 1) { totalSegments += (info.vertexCount - 1); }
	}
	if (totalSegments > 0) {
		auto* data = GetCPUSegmentData();
		saveData.segments.assign(data, data + totalSegments);
	}

	// --- JSON への保存 ---
	// FileSystemのテンプレートがよしなにJSON化してファイルに保存してくれます
	return FileSystem::SaveToFile(filename, saveData);
}

bool Hair::LoadFromFile(const std::string& filename) {
	Strands::HairSaveData loadedData{};

	// --- JSON からの読み込み ---
	// FileSystemがJSONを読み込み、構造体とvector配列に全て復元してくれます
	if (!FileSystem::LoadFromFile(filename, loadedData)) {
		std::cerr << "[Hair::Load] Failed to load JSON file: " << filename << std::endl;
		return false;
	}

	// --- マッピングメモリへの数値を直接上書き（再登録） ---
	/*if (gpuPhysicsConfigBuffer_) {
		*gpuPhysicsConfigBuffer_->GetMappedData() = loadedData.physicsConfig;
	}
	if (gpuMakeConfigBuffer_) {
		*gpuMakeConfigBuffer_->GetMappedData() = loadedData.makeConfig;
	}*/
	if (gpuConfigBuffer_) {
		*gpuConfigBuffer_->GetMappedData() = loadedData.hairConfig;
	}

	// 各バッファにmemcpyで流し込む（すでに確保されている最大サイズを超えないように std::min で制限！）
	if (uploadGuideBuffer && !loadedData.points.empty()) {

		for (auto& point : loadedData.points) {
			point.position = Matrix4x4::Transform(point.homePosition, characterMatrix_);
			point.velocity = { 0.0f,0.0f,0.0f };
		}

		size_t copyCount = (std::min)((size_t)uploadGuideBuffer->GetNumElements(), loadedData.points.size());
		std::memcpy(uploadGuideBuffer->GetMappedData(), loadedData.points.data(), copyCount * sizeof(GuideCurve::ControllerPoint));
	}
	if (guideInfoBuffer_ && !loadedData.guideInfo.empty()) {
		size_t copyCount = (std::min)((size_t)guideInfoBuffer_->GetNumElements(), loadedData.guideInfo.size());
		std::memcpy(guideInfoBuffer_->GetMappedData(), loadedData.guideInfo.data(), copyCount * sizeof(GuideCurve::GuideInfo));
	}
	if (gpuChildStrandBuffer_ && !loadedData.childStrands.empty()) {
		size_t copyCount = (std::min)((size_t)gpuChildStrandBuffer_->GetNumElements(), loadedData.childStrands.size());
		std::memcpy(gpuChildStrandBuffer_->GetMappedData(), loadedData.childStrands.data(), copyCount * sizeof(Strands::ChildStrand));
	}
	if (strandInfoBuffer_ && !loadedData.strandInfos.empty()) {
		size_t copyCount = (std::min)((size_t)strandInfoBuffer_->GetNumElements(), loadedData.strandInfos.size());
		std::memcpy(strandInfoBuffer_->GetMappedData(), loadedData.strandInfos.data(), copyCount * sizeof(Strands::StrandInfo));
	}
	if (segmentBuffer_ && !loadedData.segments.empty()) {
		size_t copyCount = (std::min)((size_t)segmentBuffer_->GetNumElements(), loadedData.segments.size());
		std::memcpy(segmentBuffer_->GetMappedData(), loadedData.segments.data(), copyCount * sizeof(Strands::SegmentData));
	}

	// メモリの書き換えが終わったので、GPUへの同期をリクエスト
	RequestNotifyUpdate();
	MarkGuideTopologyDirty();

	return true;
}

bool Hair::AddVertexToGuide(uint32_t guideIndex, const GuideCurve::ControllerPoint* customPoint) {
	if (!uploadGuideBuffer || !guideInfoBuffer_) return false;
	uint32_t totalGuideCount = GetCPUGuideInfoCount();
	if (guideIndex >= totalGuideCount) return false;

	// ガイドの情報取得
	auto* guideInfos = GetCPUGuideInfoData();
	// 頂点data
	auto* points = GetCPUGuideData();

	// 現在使用中の全頂点数と上限チェック
	uint32_t totalActivePoints = GetCPUActiveGuideCount();
	if (totalActivePoints >= uploadGuideBuffer->GetNumElements()) {
		return false; // 容量上限
	}

	// 追加するガイドの情報を取得
	GuideCurve::GuideInfo& targetGuide = guideInfos[guideIndex];
	// 追加する頂点の挿入位置を計算
	uint32_t insertIndex = targetGuide.vertexStartIndex + targetGuide.vertexCount;

	// 挿入位置以降の頂点データを1つ後ろへシフト
	for (uint32_t i = totalActivePoints; i > insertIndex; --i) {
		points[i] = points[i - 1];
	}

	// 追加する頂点データの作成
	GuideCurve::ControllerPoint newPoint{};
	// customPointが指定されていればそれをコピー、そうでなければ末尾の制御点から延伸方向を計算して補間セット
	if (customPoint) {
		newPoint = *customPoint;
	}
	else {
		// 末尾の制御点から延伸方向を計算して補間セット
		if (targetGuide.vertexCount > 0) {
			uint32_t lastIdx = insertIndex - 1;
			const auto& lastPoint = points[lastIdx];
			Vector3 dir = { 0.0f, -lastPoint.nextToLength, 0.0f };

			if (targetGuide.vertexCount > 1) {
				Vector3 prevPos = points[lastIdx - 1].position;
				Vector3 diff = { lastPoint.position.x - prevPos.x, lastPoint.position.y - prevPos.y, lastPoint.position.z - prevPos.z };
				float len = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
				if (len > 0.0001f) {
					dir = { (diff.x / len) * lastPoint.nextToLength, (diff.y / len) * lastPoint.nextToLength, (diff.z / len) * lastPoint.nextToLength };
				}
			}

			newPoint.position = { lastPoint.position.x + dir.x, lastPoint.position.y + dir.y, lastPoint.position.z + dir.z };
			newPoint.homePosition = newPoint.position;
			newPoint.radius = lastPoint.radius * 0.9f;
			newPoint.color = lastPoint.color;
			newPoint.nextToLength = lastPoint.nextToLength;
			newPoint.physicsWeight = (std::min)(1.0f, lastPoint.physicsWeight + 0.1f);
		}
	}

	// 頂点dataを挿入
	points[insertIndex] = newPoint;

	// 対象ガイドの頂点数を加算
	targetGuide.vertexCount++;

	// 後続ガイドの開始オフセットを+1更新
	for (uint32_t g = guideIndex + 1; g < totalGuideCount; ++g) {
		if (guideInfos[g].vertexCount > 0) {
			guideInfos[g].vertexStartIndex++;
		}
	}

	// GPU更新通知
	RequestNotifyUpdate();
	MarkGuideTopologyDirty();
	return true;
}

bool Hair::RemoveVertexFromGuide(uint32_t guideIndex, int pointIndexWithinGuide) {
	if (!uploadGuideBuffer || !guideInfoBuffer_) return false;
	uint32_t totalGuideCount = GetCPUGuideInfoCount();
	if (guideIndex >= totalGuideCount) return false;

	// ガイド情報の取得
	auto* guideInfos = GetCPUGuideInfoData();
	// 頂点dataを取得
	auto* points = GetCPUGuideData();
	GuideCurve::GuideInfo& targetGuide = guideInfos[guideIndex];

	// ガイドの形状維持のため最小2個の頂点は残す
	if (targetGuide.vertexCount <= 2) return false;

	// 削除対象の指定 (-1なら末尾)
	uint32_t removeOffset = (pointIndexWithinGuide < 0 || pointIndexWithinGuide >= static_cast<int>(targetGuide.vertexCount))
		? (targetGuide.vertexCount - 1)
		: static_cast<uint32_t>(pointIndexWithinGuide);

	uint32_t removeIndex = targetGuide.vertexStartIndex + removeOffset;
	uint32_t totalActivePoints = GetCPUActiveGuideCount();

	// 削除位置以降の頂点データを1つ前へシフト
	for (uint32_t i = removeIndex; i < totalActivePoints - 1; ++i) {
		points[i] = points[i + 1];
	}

	// 対象ガイドの頂点数を減算
	targetGuide.vertexCount--;

	// 後続ガイドの開始オフセットを-1更新
	for (uint32_t g = guideIndex + 1; g < totalGuideCount; ++g) {
		if (guideInfos[g].vertexCount > 0) {
			guideInfos[g].vertexStartIndex--;
		}
	}

	// GPU更新通知
	RequestNotifyUpdate();
	MarkGuideTopologyDirty();
	return true;
}

uint32_t Hair::GetCPUActiveGuideCount() {
	// 構造が変わっている場合のみ再計算する
	if (isGuideTopologyDirty_) {
		RecalculateActiveVertexCount();
		isGuideTopologyDirty_ = false;
	}
	return cachedActiveVertexCount_;
}

void Hair::RecalculateActiveVertexCount() {
	uint32_t totalVertices = 0;

	if (guideInfoBuffer_) {
		// バッファからデータを取り出して集計
		auto* mappedData = static_cast<const GuideCurve::GuideInfo*>(guideInfoBuffer_->GetMappedData());
		const uint32_t numElements = guideInfoBuffer_->GetNumElements();

		for (uint32_t i = 0; i < numElements; ++i) {
			if (mappedData[i].vertexCount == 0) {
				break; // 有効なガイドの終端
			}
			totalVertices += mappedData[i].vertexCount;
		}
	}

	cachedActiveVertexCount_ = totalVertices;
}