#include "RayTracingManager.h"
#include "CameraSystem.h"
#include "ModelManager.h"
#include "SkinningManager.h"
#include "../../Objects/Hair/IHair.h"
#include <d3dx12.h>
#include <pix3.h>

RayTracingManager::~RayTracingManager() {
	// 登録インスタンスのクリア
	registeredInstances_.clear();

	// マップに保持している全 BLAS リソースを明示的にクリア（これで ComPtr が解放される）
	dynamicBLASMap_.clear();
	staticBLASMap_.clear();

	// TLAS および各バッファの解放
	tlasResultBuffer_.Reset();
	tlasScratchBuffer_.Reset();
	instanceDescBuffer_.reset();
	materialStructuredBuffer_.reset();

	// パイプライン・SBT・ヒープ等の解放
	sbtBuffer_.Reset();
	rtpso_.Reset();
	globalRootSignature_.Reset();
	outputTexture_.reset();
	cameraBuffer_.reset();
	rtDescriptorHeap_.Reset();

	// コマンドリストの参照解除
	dxrCmdList_.Reset();

	device5_.Reset();
}

constexpr UINT AlignTo(UINT size, UINT alignment) {
	return(size + alignment - 1) & ~(alignment - 1);
}

void RayTracingManager::Initialize(Fngine* engine) {

	engine_ = engine;
	width_ = 1280;
	height_ = 720;

	ID3D12Device* pDevice = engine_->GetD3D12System().GetDevice().Get();
	HRESULT hr = pDevice->QueryInterface(IID_PPV_ARGS(&device5_));
	assert(SUCCEEDED(hr) && "Device does not support DXR (ID3D12Device5)");

	auto commandList = engine_->GetCommand().GetList().GetList();
	// ■【 RayTracingを使用するために4にキャストしたコマンドリストを取得 】
	commandList.As(&dxrCmdList_);

	descriptorSize_ = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// 出力キャンパステクスチャの生成
	outputTexture_ = std::make_unique<RWTexture2D>();
	outputTexture_->Initialize(engine_, width_, height_, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	outputTexture_->GetResource()->SetName(L"RTManager_OutPutTexture");
	outputUAVHandleGPU_ = outputTexture_->GetUAVHandleGPU();

	// カメラ
	cameraBuffer_ = std::make_unique<ConstantBuffer<RayTracingCamera>>(engine_);
	cameraBuffer_->Initialize();

	// RootSignature & RTPSO & SBT の構築
	CreateDescriptorHeap();
	CreateRootSignature();
	CreateRaytracingPSO();
	CreateShaderBindingTable();

	instanceDescBuffer_ = std::make_unique<Structured<D3D12_RAYTRACING_INSTANCE_DESC>>(engine_);
	instanceDescBuffer_->Initialize(2048);
	instanceDescBuffer_->GetResource()->SetName(L"RTManager_InstanceDescBuffer");

	materialStructuredBuffer_ = std::make_unique<Structured<RayTracingInstanceMaterial>>(engine_);
	materialStructuredBuffer_->Initialize(2048);
	materialStructuredBuffer_->GetResource()->SetName(L"RTManager_MaterialStructuredBuffer");
}

// ====================
// 【 更新関係の処理 】
// ====================

// AllUpdate,____
void RayTracingManager::Update(float deltaTime)
{
	if (registeredInstances_.empty()) return;

	UINT numInstances = static_cast<UINT>(registeredInstances_.size());

	auto* instanceDescs = instanceDescBuffer_->GetMappedData();
	auto* materials = materialStructuredBuffer_->GetMappedData();

	UINT currentHairBufferCount = 0;

	for (UINT i = 0; i < numInstances; ++i) {
		const auto& inst = registeredInstances_[i];

		// BLAS の取得または動的ビルド
		D3D12_GPU_VIRTUAL_ADDRESS blasAddr = 0;
		if (inst.type == RayTracingInstanceType::StaticMesh) {
			blasAddr = GetOrCreateStaticBLAS(inst.modelID);
		}
		else {
			// インスタンスが持つ固有のリソースポインタをキーにして渡す
			ID3D12Resource* keyResource = (inst.type == RayTracingInstanceType::HairProcedural)
				? inst.hairFlatVertexBuffer
				: inst.vertexBufferResource;

			blasAddr = BuildDynamicBLAS(inst, keyResource);
		}

		// TLAS インスタンス記述子の設定
		D3D12_RAYTRACING_INSTANCE_DESC desc{};
		Matrix4x4 transposeMat = Matrix4x4::Transpose(inst.worldMatrix);
		std::memcpy(desc.Transform, &transposeMat, sizeof(desc.Transform));

		desc.InstanceID = i;
		desc.InstanceMask = 0xFF;

		// HitGroup の基本オフセット指定（※ HairShadowAnyHit 導入時は 2、未導入時は 1）
		desc.InstanceContributionToHitGroupIndex = (inst.type == RayTracingInstanceType::HairProcedural) ? 1 : 0;
		desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		desc.AccelerationStructure = blasAddr;

		instanceDescs[i] = desc;

		// マテリアルパラメータおよび SRV の設定
		RayTracingInstanceMaterial matData{};
		matData.color = inst.color;
		matData.isHair = (inst.type == RayTracingInstanceType::HairProcedural) ? 1 : 0;

		// 髪の毛パーツの場合、rtDescriptorHeap_ の Index 2〜 に SRV を直接生成
		if (inst.type == RayTracingInstanceType::HairProcedural && inst.hairFlatVertexBuffer != nullptr) {
			matData.hairBufferIndex = currentHairBufferCount;

			// Index 0 (Output UAV) と Index 1 (Depth SRV) を避けて Index 2 から書き込む
			CD3DX12_CPU_DESCRIPTOR_HANDLE targetHandle(
				rtDescriptorHeap_->GetCPUDescriptorHandleForHeapStart(),
				2 + currentHairBufferCount,
				descriptorSize_
			);

			// SRV の設定を構築
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.NumElements = inst.aabbCount * 2; // 1 AABB = 2 頂点 (StrandVertex)
			srvDesc.Buffer.StructureByteStride = sizeof(Strands::StrandVertex);
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

			// rtDescriptorHeap_ 内に SRV を直接書き込み
			device5_->CreateShaderResourceView(inst.hairFlatVertexBuffer, &srvDesc, targetHandle);

			currentHairBufferCount++;
		}
		else {
			matData.hairBufferIndex = 0;
		}

		materials[i] = matData;
	}

	// TLAS (Top-Level Acceleration Structure) のビルド
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs{};
	tlasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	tlasInputs.NumDescs = numInstances;
	tlasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	tlasInputs.InstanceDescs = instanceDescBuffer_->GetResource()->GetGPUVirtualAddress();

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasPrebuildInfo{};
	device5_->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasPrebuildInfo);

	// TLAS バッファの容量確保・再生成
	if (!tlasResultBuffer_ || tlasResultBuffer_->GetDesc().Width < tlasPrebuildInfo.ResultDataMaxSizeInBytes) {
		tlasScratchBuffer_ = CreateBufferResource(engine_->GetD3D12System().GetDevice().Get(), tlasPrebuildInfo.ScratchDataSizeInBytes, true);
		tlasScratchBuffer_->SetName(L"RTManager_TLAS_ScratchBuffer");

		D3D12_HEAP_PROPERTIES heapProps{ D3D12_HEAP_TYPE_DEFAULT };
		D3D12_RESOURCE_DESC resDesc{};
		resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resDesc.Width = tlasPrebuildInfo.ResultDataMaxSizeInBytes;
		resDesc.Height = 1;
		resDesc.DepthOrArraySize = 1;
		resDesc.MipLevels = 1;
		resDesc.Format = DXGI_FORMAT_UNKNOWN;
		resDesc.SampleDesc.Count = 1;
		resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		device5_->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
			nullptr,
			IID_PPV_ARGS(&tlasResultBuffer_)
		);
		tlasResultBuffer_->SetName(L"RTManager_TLAS_ResultBuffer");
	}

	// TLAS ビルドコマンドの発行
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasBuildDesc{};
	tlasBuildDesc.Inputs = tlasInputs;
	tlasBuildDesc.DestAccelerationStructureData = tlasResultBuffer_->GetGPUVirtualAddress();
	tlasBuildDesc.ScratchAccelerationStructureData = tlasScratchBuffer_->GetGPUVirtualAddress();

	dxrCmdList_->BuildRaytracingAccelerationStructure(&tlasBuildDesc, 0, nullptr);

	// TLAS ビルド完了待ち UAV バリア
	D3D12_RESOURCE_BARRIER tlasBarrier{};
	tlasBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	tlasBarrier.UAV.pResource = tlasResultBuffer_.Get();
	dxrCmdList_->ResourceBarrier(1, &tlasBarrier);
}

D3D12_GPU_VIRTUAL_ADDRESS RayTracingManager::GetOrCreateStaticBLAS(uint32_t modelID) {
	auto it = staticBLASMap_.find(modelID);
	if (it != staticBLASMap_.end()) {
		return it->second.resultBuffer->GetGPUVirtualAddress();
	}

	auto& objectData = ModelManager::GetInstance()->GetObjectData(modelID);
	if (objectData.GetVertexCount() == 0) return 0;

	// 静的メッシュの VB / IB は GENERIC_READ 状態のため、バリア発行は一切不要！

	D3D12_RAYTRACING_GEOMETRY_DESC geomDesc{};
	geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
	geomDesc.Triangles.VertexBuffer.StartAddress = objectData.GetVertexBufferView().BufferLocation;
	geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(VertexData);
	geomDesc.Triangles.VertexCount = objectData.GetVertexCount();
	geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geomDesc.Triangles.IndexBuffer = objectData.GetIndexBufferView().BufferLocation;
	geomDesc.Triangles.IndexCount = objectData.GetIndexCount();
	geomDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	inputs.NumDescs = 1;
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.pGeometryDescs = &geomDesc;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo{};
	device5_->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);

	BLASResource res{};
	res.scratchBuffer = CreateBufferResource(engine_->GetD3D12System().GetDevice().Get(), prebuildInfo.ScratchDataSizeInBytes, true);
	res.scratchBuffer->SetName(L"StaticBLAS_ScratchBuffer");

	D3D12_HEAP_PROPERTIES heapProps{ D3D12_HEAP_TYPE_DEFAULT };
	D3D12_RESOURCE_DESC resDesc{};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = prebuildInfo.ResultDataMaxSizeInBytes;
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.Format = DXGI_FORMAT_UNKNOWN;
	resDesc.SampleDesc.Count = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	device5_->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, IID_PPV_ARGS(&res.resultBuffer));
	res.resultBuffer->SetName(L"StaticBLAS_ResultBuffer");

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc{};
	buildDesc.Inputs = inputs;
	buildDesc.DestAccelerationStructureData = res.resultBuffer->GetGPUVirtualAddress();
	buildDesc.ScratchAccelerationStructureData = res.scratchBuffer->GetGPUVirtualAddress();

	dxrCmdList_->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

	D3D12_RESOURCE_BARRIER uavBarrier{};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = res.resultBuffer.Get();
	dxrCmdList_->ResourceBarrier(1, &uavBarrier);

	staticBLASMap_[modelID] = res;
	return res.resultBuffer->GetGPUVirtualAddress();
}

D3D12_GPU_VIRTUAL_ADDRESS RayTracingManager::BuildDynamicBLAS(const RayTracingInstanceDesc& inst, ID3D12Resource* keyResource) {
	if (!keyResource) return 0;

	// インデックス依存を廃止し、固有リソースポインタをキーにして BLAS を取得・自動生成
	auto& res = dynamicBLASMap_[keyResource];

	std::vector<D3D12_RESOURCE_BARRIER> preBarriers;
	std::vector<D3D12_RESOURCE_BARRIER> postBarriers;

	// インスタンス種別ごとに実際の状態に合わせてステート遷移を分岐
	if (inst.type == RayTracingInstanceType::SkinnedMesh) {
		if (inst.vertexBufferResource) {
			preBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
				inst.vertexBufferResource,
				D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
			));
			postBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
				inst.vertexBufferResource,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
			));
		}
	}
	//else if (inst.type == RayTracingInstanceType::HairProcedural) {
	//	// AABBバッファ（またはFlatVertexBuffer）のステート遷移
	//	ID3D12Resource* targetRes = inst.vertexBufferResource ? inst.vertexBufferResource : inst.hairFlatVertexBuffer;
	//	if (targetRes) {
	//		preBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
	//			targetRes,
	//			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
	//			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
	//		));
	//		postBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
	//			targetRes,
	//			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
	//			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	//		));
	//	}
	//}

	if (!preBarriers.empty()) {
		dxrCmdList_->ResourceBarrier(static_cast<UINT>(preBarriers.size()), preBarriers.data());
	}

	D3D12_RAYTRACING_GEOMETRY_DESC geomDesc{};

	if (inst.type == RayTracingInstanceType::SkinnedMesh) {
		geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
		geomDesc.Triangles.VertexBuffer.StartAddress = inst.vertexBufferGPU;
		geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(VertexData);
		geomDesc.Triangles.VertexCount = inst.vertexCount;
		geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		geomDesc.Triangles.IndexBuffer = inst.indexBufferGPU;
		geomDesc.Triangles.IndexCount = inst.indexCount;
		geomDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
	}
	else if (inst.type == RayTracingInstanceType::HairProcedural) {
		geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
		geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
		geomDesc.AABBs.AABBCount = inst.aabbCount;
		geomDesc.AABBs.AABBs.StartAddress = inst.vertexBufferGPU;
		geomDesc.AABBs.AABBs.StrideInBytes = sizeof(D3D12_RAYTRACING_AABB);
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	inputs.NumDescs = 1;
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.pGeometryDescs = &geomDesc;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo{};
	device5_->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);

	// 未作成または容量不足の場合のみバッファを新規生成（作成済みなら100%使い回す）
	if (!res.resultBuffer || res.resultBuffer->GetDesc().Width < prebuildInfo.ResultDataMaxSizeInBytes) {
		res.scratchBuffer = CreateBufferResource(engine_->GetD3D12System().GetDevice().Get(), prebuildInfo.ScratchDataSizeInBytes, true);
		res.scratchBuffer->SetName(L"DynamicBLAS_ScratchBuffer");

		D3D12_HEAP_PROPERTIES heapProps{ D3D12_HEAP_TYPE_DEFAULT };
		D3D12_RESOURCE_DESC resDesc{};
		resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resDesc.Width = prebuildInfo.ResultDataMaxSizeInBytes;
		resDesc.Height = 1;
		resDesc.DepthOrArraySize = 1;
		resDesc.MipLevels = 1;
		resDesc.Format = DXGI_FORMAT_UNKNOWN;
		resDesc.SampleDesc.Count = 1;
		resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		device5_->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
			nullptr,
			IID_PPV_ARGS(&res.resultBuffer)
		);
		res.resultBuffer->SetName(L"DynamicBLAS_ResultBuffer");
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc{};
	buildDesc.Inputs = inputs;
	buildDesc.DestAccelerationStructureData = res.resultBuffer->GetGPUVirtualAddress();
	buildDesc.ScratchAccelerationStructureData = res.scratchBuffer->GetGPUVirtualAddress();

	dxrCmdList_->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

	D3D12_RESOURCE_BARRIER uavBarrier{};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = res.resultBuffer.Get();
	dxrCmdList_->ResourceBarrier(1, &uavBarrier);

	if (!postBarriers.empty()) {
		dxrCmdList_->ResourceBarrier(static_cast<UINT>(postBarriers.size()), postBarriers.data());
	}

	return res.resultBuffer->GetGPUVirtualAddress();
}

// ============================
// 【 インスタンスの登録関係 】
// ============================

// 解除,____
void RayTracingManager::ClearInstance() {
	registeredInstances_.clear();
}

// Static Mesh,____
void RayTracingManager::RegisterStaticMeshInstance(uint32_t modelID, const Matrix4x4& worldMat, const Vector4& color) {
	RayTracingInstanceDesc desc{};
	desc.type = RayTracingInstanceType::StaticMesh;
	desc.modelID = modelID;
	desc.worldMatrix = worldMat;
	desc.color = color;
	registeredInstances_.push_back(desc);
}

// Skinned Mesh,____
void RayTracingManager::RegisterSkinnedMeshInstance(
	D3D12_GPU_VIRTUAL_ADDRESS vertexBufferGPU,D3D12_GPU_VIRTUAL_ADDRESS indexBufferGPU,
	UINT vertexCount,UINT indexCount,
	const Matrix4x4& transform,const Vector4& color,
	ID3D12Resource* vertexBufferResource, ID3D12Resource* indexBufferResource
)
{
	RayTracingInstanceDesc desc{};
	desc.type = RayTracingInstanceType::SkinnedMesh;
	desc.vertexBufferGPU = vertexBufferGPU;
	desc.indexBufferGPU = indexBufferGPU;
	desc.vertexCount = vertexCount;
	desc.indexCount = indexCount;
	desc.worldMatrix = transform;
	desc.color = color;
	desc.vertexBufferResource = vertexBufferResource;
	desc.indexBufferResource = indexBufferResource;
	registeredInstances_.push_back(desc);
}

// Hair,____
void RayTracingManager::RegisterHairInstance(
	D3D12_GPU_VIRTUAL_ADDRESS aabbBufferGPU,
	ID3D12Resource* flatVertexBufferResource,
	UINT aabbCount,
	const Matrix4x4& worldMat,
	const Vector4& color)
{
	RayTracingInstanceDesc desc{};
	desc.type = RayTracingInstanceType::HairProcedural;
	desc.vertexBufferGPU = aabbBufferGPU;
	desc.hairFlatVertexBuffer = flatVertexBufferResource;
	desc.aabbCount = aabbCount;
	desc.worldMatrix = worldMat;
	desc.color = color;
	registeredInstances_.push_back(desc);
}

// ==================
// 【 レンダリング 】
// ==================

void RayTracingManager::Render(ID3D12Resource* depthResource)
{
#ifdef USE_IMGUI
	// PIXイベントを開始
	PIXScopedEvent(dxrCmdList_.Get(), PIX_COLOR_INDEX(2), "RayTracing Render Pass");
#endif// USEIMGUI
	if (registeredInstances_.empty() || !tlasResultBuffer_) return;

	// -------------------------------------------------------------
	// rtDescriptorHeap_ の Index 0 と 1 に Output UAV と Depth SRV を直接構築
	// -------------------------------------------------------------
	CD3DX12_CPU_DESCRIPTOR_HANDLE heapCpuStart(rtDescriptorHeap_->GetCPUDescriptorHandleForHeapStart());

	// [Index 0] Output UAV
	CD3DX12_CPU_DESCRIPTOR_HANDLE uavCpuHandle(heapCpuStart, 0, descriptorSize_);
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	device5_->CreateUnorderedAccessView(outputTexture_->GetResource(), nullptr, &uavDesc, uavCpuHandle);

	// [Index 1] Depth SRV
	CD3DX12_CPU_DESCRIPTOR_HANDLE depthCpuHandle(heapCpuStart, 1, descriptorSize_);
	D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvDesc{};

	depthSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	depthSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	depthSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	depthSrvDesc.Texture2D.MipLevels = 1;

	device5_->CreateShaderResourceView(depthResource, &depthSrvDesc, depthCpuHandle);

	// -------------------------------------------------------------
	// カメラ等のパラメータ更新
	// -------------------------------------------------------------
	auto camera = CameraSystem::GetInstance()->GetActiveCamera();
	RayTracingCamera camData;
	camData.inverseViewProj = Matrix4x4::Inverse(camera->GetViewProjectionMatrix());
	camData.position = camera->GetTranslation();
	std::memcpy(cameraBuffer_->GetMappedData(), &camData, sizeof(RayTracingCamera));

	// -------------------------------------------------------------
	// レイトレ専用 Descriptor Heap をバインド（これ1つのみでOK！）
	// -------------------------------------------------------------
	ID3D12DescriptorHeap* ppHeaps[] = { rtDescriptorHeap_.Get() };
	dxrCmdList_->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	CD3DX12_GPU_DESCRIPTOR_HANDLE heapGpuStart(rtDescriptorHeap_->GetGPUDescriptorHandleForHeapStart());

	dxrCmdList_->SetComputeRootSignature(globalRootSignature_.Get());
	dxrCmdList_->SetComputeRootConstantBufferView(0, cameraBuffer_->GetGPUVirtualAddress());
	dxrCmdList_->SetComputeRootShaderResourceView(1, tlasResultBuffer_->GetGPUVirtualAddress());

	// Index 0 の Output UAV ハンドル
	dxrCmdList_->SetComputeRootDescriptorTable(2, CD3DX12_GPU_DESCRIPTOR_HANDLE(heapGpuStart, 0, descriptorSize_));

	// Index 1 の Depth SRV ハンドル
	dxrCmdList_->SetComputeRootDescriptorTable(3, CD3DX12_GPU_DESCRIPTOR_HANDLE(heapGpuStart, 1, descriptorSize_));

	dxrCmdList_->SetComputeRootShaderResourceView(4, materialStructuredBuffer_->GetResource()->GetGPUVirtualAddress());

	// Index 2～ の Hair Flat Vertices SRV 配列の先頭ハンドル
	dxrCmdList_->SetComputeRootDescriptorTable(5, CD3DX12_GPU_DESCRIPTOR_HANDLE(heapGpuStart, 2, descriptorSize_));

	dxrCmdList_->SetPipelineState1(rtpso_.Get());

	dispatchDesc_.Width = width_;
	dispatchDesc_.Height = height_;
	dispatchDesc_.Depth = 1;

	dxrCmdList_->DispatchRays(&dispatchDesc_);
}

void RayTracingManager::PreRender(ID3D12Resource* renderTargetResource, D3D12_RESOURCE_STATES preState, ID3D12Resource* depthResource)
{
#ifdef USE_IMGUI
	// PIXイベントを開始
	PIXScopedEvent(dxrCmdList_.Get(), PIX_COLOR_INDEX(2), "RayTracing Pre Pass");
#endif// USEIMGUI
	D3D12_RESOURCE_BARRIER barriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(renderTargetResource, preState, D3D12_RESOURCE_STATE_COPY_SOURCE),
		CD3DX12_RESOURCE_BARRIER::Transition(outputTexture_->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST),
		CD3DX12_RESOURCE_BARRIER::Transition(depthResource, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
	};
	dxrCmdList_->ResourceBarrier(_countof(barriers), barriers);
	dxrCmdList_->CopyResource(outputTexture_->GetResource(), renderTargetResource);

	D3D12_RESOURCE_BARRIER postBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(outputTexture_->GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		CD3DX12_RESOURCE_BARRIER::Transition(renderTargetResource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST)
	};
	dxrCmdList_->ResourceBarrier(_countof(postBarriers), postBarriers);
}

void RayTracingManager::PostRender(ID3D12Resource* renderTargetResource, D3D12_RESOURCE_STATES postState, ID3D12Resource* depthResource)
{
#ifdef USE_IMGUI
	// PIXイベントを開始
	PIXScopedEvent(dxrCmdList_.Get(), PIX_COLOR_INDEX(2), "RayTracing Post Pass");
#endif// USEIMGUI
	D3D12_RESOURCE_BARRIER srcBarrier = CD3DX12_RESOURCE_BARRIER::Transition(outputTexture_->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	dxrCmdList_->ResourceBarrier(1, &srcBarrier);

	dxrCmdList_->CopyResource(renderTargetResource, outputTexture_->GetResource());

	D3D12_RESOURCE_BARRIER finalBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(renderTargetResource, D3D12_RESOURCE_STATE_COPY_DEST, postState),
		CD3DX12_RESOURCE_BARRIER::Transition(outputTexture_->GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		CD3DX12_RESOURCE_BARRIER::Transition(depthResource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE)
	};
	dxrCmdList_->ResourceBarrier(_countof(finalBarriers), finalBarriers);
}


// ==============================
// 【 シェーダー・PSO・SBT構築 】
// ==============================

void RayTracingManager::CreateDescriptorHeap() {
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = TOTAL_DESCRIPTORS;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask = 0;

	HRESULT hr = device5_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtDescriptorHeap_));
	assert(SUCCEEDED(hr));
	rtDescriptorHeap_->SetName(L"RayTracing Descriptor Heap");
}

void RayTracingManager::CreateRootSignature()
{
	ID3D12Device* pDevice = engine_->GetD3D12System().GetDevice().Get();

	CD3DX12_ROOT_PARAMETER rootParameters[5]{};
	rootParameters[0].InitAsConstantBufferView(0); // b0: カメラ情報
	rootParameters[1].InitAsShaderResourceView(0); // t0: TLAS

	CD3DX12_DESCRIPTOR_RANGE uavRange;
	uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // u0: 出力キャンバス
	rootParameters[2].InitAsDescriptorTable(1, &uavRange);

	CD3DX12_DESCRIPTOR_RANGE srvDepthRange;
	srvDepthRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // t1: 深度テクスチャ
	rootParameters[3].InitAsDescriptorTable(1, &srvDepthRange);

	rootParameters[4].InitAsShaderResourceView(2);// t2: Material Buffer

	CD3DX12_DESCRIPTOR_RANGE hairSrvRange;
	hairSrvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MAX_HAIR_BUFFERS, 3); // register(t3) から最大64個
	CD3DX12_ROOT_PARAMETER rootParametersFinal[6]{};
	rootParametersFinal[0] = rootParameters[0];
	rootParametersFinal[1] = rootParameters[1];
	rootParametersFinal[2] = rootParameters[2];
	rootParametersFinal[3] = rootParameters[3];
	rootParametersFinal[4] = rootParameters[4];
	rootParametersFinal[5].InitAsDescriptorTable(1, &hairSrvRange);// t3: HairFlatVertices Descriptor Table

	CD3DX12_ROOT_SIGNATURE_DESC globalRootSigDesc(ARRAYSIZE(rootParametersFinal), rootParametersFinal);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob, errorBlob;
	D3D12SerializeRootSignature(&globalRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (errorBlob) {
		Log::View((char*)errorBlob->GetBufferPointer());
		assert(false);
	}
	pDevice->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&globalRootSignature_));

	globalRootSignature_->SetName(L"RayTracing Global Root Signature");
}

void RayTracingManager::CreateRaytracingPSO()
{
	raytracingShaderBlob_ = CompileShader(L"resources/shaders/Hair/HairRaytracing.hlsl", L"lib_6_5");

	CD3DX12_STATE_OBJECT_DESC pipelineDesc(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

	// DXIL Library 登録
	auto libSubobject = pipelineDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE(raytracingShaderBlob_->GetBufferPointer(), raytracingShaderBlob_->GetBufferSize());
	libSubobject->SetDXILLibrary(&libdxil);
	libSubobject->DefineExport(L"MyRayGenShader");
	libSubobject->DefineExport(L"MyMissShader");
	libSubobject->DefineExport(L"ShadowMissShader");
	libSubobject->DefineExport(L"MeshClosestHitShader");
	libSubobject->DefineExport(L"HairIntersectionShader");
	libSubobject->DefineExport(L"HairClosestHitShader");

	// Hit Groups 登録 (SBT のインデックスと対応させる)
	// HitGroup 0: Mesh (Triangle)
	auto meshHitGroup = pipelineDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	meshHitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
	meshHitGroup->SetHitGroupExport(L"MeshHitGroup");
	meshHitGroup->SetClosestHitShaderImport(L"MeshClosestHitShader");

	// HitGroup 1: Hair (Procedural AABB)
	auto hairHitGroup = pipelineDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	hairHitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE);
	hairHitGroup->SetHitGroupExport(L"HairHitGroup");
	hairHitGroup->SetIntersectionShaderImport(L"HairIntersectionShader");
	hairHitGroup->SetClosestHitShaderImport(L"HairClosestHitShader");

	// Shader Config
	auto shaderConfig = pipelineDesc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	shaderConfig->Config(sizeof(float) * 4, sizeof(float) * 5); // Payload, Attributes

	// Global Root Signature
	auto globalRootSig = pipelineDesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	globalRootSig->SetRootSignature(globalRootSignature_.Get());

	// Pipeline Config (再帰深度 = 2: シャドウレイ対応)
	auto pipelineConfig = pipelineDesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	pipelineConfig->Config(2);

	HRESULT hr = device5_->CreateStateObject(pipelineDesc, IID_PPV_ARGS(&rtpso_));
	assert(SUCCEEDED(hr) && "Failed to create Raytracing Pipeline State Object");
}

void RayTracingManager::CreateShaderBindingTable()
{
	ID3D12Device* pDevice = engine_->GetD3D12System().GetDevice().Get();

	Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> rtpsoProps;
	rtpso_->QueryInterface(IID_PPV_ARGS(&rtpsoProps));
	rtpso_->SetName(L"RayTracing Pipeline State Object");

	// 各識別子の取得
	void* rayGenId = rtpsoProps->GetShaderIdentifier(L"MyRayGenShader");
	void* missId = rtpsoProps->GetShaderIdentifier(L"MyMissShader");
	void* shadowMissId = rtpsoProps->GetShaderIdentifier(L"ShadowMissShader");
	void* meshHitId = rtpsoProps->GetShaderIdentifier(L"MeshHitGroup");
	void* hairHitId = rtpsoProps->GetShaderIdentifier(L"HairHitGroup");

	UINT shaderIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	UINT recordSize = AlignTo(shaderIdSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

	// RayGen: 1 record, Miss: 2 records (Color, Shadow), HitGroup: 2 records (Mesh, Hair)
	UINT rayGenSectionSize = AlignTo(recordSize * 1, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
	UINT missSectionSize = AlignTo(recordSize * 2, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
	UINT hitGroupSectionSize = AlignTo(recordSize * 2, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

	UINT totalSBTSize = rayGenSectionSize + missSectionSize + hitGroupSectionSize;

	sbtBuffer_ = CreateBufferResource(pDevice, totalSBTSize);
	sbtBuffer_->SetName(L"RayTracing SBT Buffer");

	uint8_t* pData = nullptr;
	sbtBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&pData));
	std::memset(pData, 0, totalSBTSize);

	uint8_t* pRayGenHeader = pData;
	uint8_t* pMissHeader = pRayGenHeader + rayGenSectionSize;
	uint8_t* pHitGroupHeader = pMissHeader + missSectionSize;

	// 書き込み
	std::memcpy(pRayGenHeader, rayGenId, shaderIdSize);

	std::memcpy(pMissHeader + 0 * recordSize, missId, shaderIdSize);
	std::memcpy(pMissHeader + 1 * recordSize, shadowMissId, shaderIdSize);

	// SBT HitGroup Table: Index 0 = Mesh, Index 1 = Hair
	std::memcpy(pHitGroupHeader + 0 * recordSize, meshHitId, shaderIdSize);
	std::memcpy(pHitGroupHeader + 1 * recordSize, hairHitId, shaderIdSize);

	sbtBuffer_->Unmap(0, nullptr);

	D3D12_GPU_VIRTUAL_ADDRESS sbtAddress = sbtBuffer_->GetGPUVirtualAddress();

	dispatchDesc_.RayGenerationShaderRecord.StartAddress = sbtAddress;
	dispatchDesc_.RayGenerationShaderRecord.SizeInBytes = recordSize;

	dispatchDesc_.MissShaderTable.StartAddress = sbtAddress + rayGenSectionSize;
	dispatchDesc_.MissShaderTable.SizeInBytes = recordSize * 2;
	dispatchDesc_.MissShaderTable.StrideInBytes = recordSize;

	dispatchDesc_.HitGroupTable.StartAddress = sbtAddress + rayGenSectionSize + missSectionSize;
	dispatchDesc_.HitGroupTable.SizeInBytes = recordSize * 2;
	dispatchDesc_.HitGroupTable.StrideInBytes = recordSize;
}

Microsoft::WRL::ComPtr<IDxcBlob> RayTracingManager::CompileShader(const std::wstring& filePath, const wchar_t* profile) {
	auto& dxc = engine_->GetDXC();
	Log::View(ConvertString(std::format(L"Begin DXR CompileShader, path:{}, profile:{}\n", filePath, profile)));

	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxc.GetUtils()->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	assert(SUCCEEDED(hr));

	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;

	// レイトレ（ライブラリ）用のコンパイル
	std::vector<LPCWSTR> arguments = {
		filePath.c_str(),
		L"-T", profile,          // L"lib_6_5" などを指定
		L"-HV", L"2021",         // float32_t 等を使うためにHLSL 2021規格を有効化
		L"-Zi", L"-Qembed_debug",// デバッグ用
		L"-Od",                  // 最適化オフ（デバッグしやすくする）
		L"-Zpr",                 // 行優先（Row Major）
	};

	IDxcResult* shaderResult = nullptr;
	hr = dxc.GetCompiler()->Compile(
		&shaderSourceBuffer,
		arguments.data(),
		static_cast<UINT32>(arguments.size()),
		dxc.GetIncludeHandle().Get(),
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