#pragma once
#include "ISingleton.h"
#include "Structured.h"
#include "Constant.h"


struct RayTracingCamera {
	Matrix4x4 inverseViewProj;
	Vector3 position;
	float padding = 0.0f;
};

struct RayTracingInstanceMaterial {
	Vector4 color;
	uint32_t isHair;         // 0:三角形メッシュ 1:Hair Procedural
	uint32_t hairBufferIndex;// HairFlatVerticesのインデックス
	float padding[2];
};

enum class RayTracingInstanceType {
	StaticMesh,   // 静的メッシュ
	SkinnedMesh,  // 動的スキニングメッシュ
	HairProcedural// カプセルAABB髪の毛
};

struct RayTracingInstanceDesc {
	RayTracingInstanceType type;
	uint32_t modelID;                              // StaticなMesh用
	D3D12_GPU_VIRTUAL_ADDRESS vertexBufferGPU;     // SkinnedMesh / Hair AABB 用
	D3D12_GPU_VIRTUAL_ADDRESS indexBufferGPU;      // SkinnedMesh用
	ID3D12Resource* vertexBufferResource = nullptr;// SkinnedMesh / Hair AABB 用 リソース
	ID3D12Resource* indexBufferResource = nullptr; // SkinnedMesh 用 インデックスリソース
	ID3D12Resource* hairFlatVertexBuffer = nullptr;
	uint32_t vertexCount;
	uint32_t indexCount;
	uint32_t aabbCount;// Hair用
	Matrix4x4 worldMatrix;
	Vector4 color;
};

class RayTracingManager :
	public ISingleton<RayTracingManager>
{
public:
	friend class ISingleton<RayTracingManager>;
	RayTracingManager() = default;
	~RayTracingManager();
public:
	void Initialize(Fngine* engine);

	// ============================
	// 【 インスタンスの登録関係 】
	// ============================

	// Static Mesh,____
	void RegisterStaticMeshInstance(
		uint32_t modelID,
		const Matrix4x4& worldMat,
		const Vector4& color = Vector4(1.0f, 1.0f, 1.0f, 1.0f)
	);

	// Skinned Mesh,____
	void RegisterSkinnedMeshInstance(
		D3D12_GPU_VIRTUAL_ADDRESS vertexBufferGPU,
		D3D12_GPU_VIRTUAL_ADDRESS indexBufferGPU,
		UINT vertexCount,
		UINT indexCount,
		const Matrix4x4& transform = Matrix4x4::Make::Identity(),
		const Vector4& color = Vector4(1.0f, 1.0f, 1.0f, 1.0f),
		ID3D12Resource* vertexBufferResource = nullptr,
		ID3D12Resource* indexBufferResource = nullptr
	);

	// Procedural,____
	void RegisterHairInstance(
		D3D12_GPU_VIRTUAL_ADDRESS aabbBufferGPU,
		ID3D12Resource* flatVertexBuffer,
		UINT aabbCount,
		const Matrix4x4& worldMat = Matrix4x4::Make::Identity(),
		const Vector4& color = Vector4(1.0f, 1.0f, 1.0f, 1.0f)
	);

	// 解除,____
	void ClearInstance();

	// =================
	// 【 更新 & 描画 】
	// =================

	// インスタンスの行列の更新
	void UpdateInstanceTransform(UINT instanceID, const Matrix4x4& transform);
	// 各BLAS,TLASの更新処理
	void Update(float deltaTime);
	// レイトレーシングの実行（DispatchRays）
	void Render(ID3D12Resource* depthResource);
	// 前処理と後処理
	void PreRender(ID3D12Resource* renderTargetResource, D3D12_RESOURCE_STATES preState, ID3D12Resource* depthResource);
	void PostRender(ID3D12Resource* renderTargetResource, D3D12_RESOURCE_STATES postState, ID3D12Resource* depthResource);

	// ===========================
	// 【 ゲッター / リクエスト 】
	// ===========================

private:
	// シェーダー・PSO・SBT構築
	void CreateRootSignature();
	void CreateRaytracingPSO();
	void CreateShaderBindingTable();
	void CreateDescriptorHeap();

	// 内部ビルド処理
	D3D12_GPU_VIRTUAL_ADDRESS GetOrCreateStaticBLAS(uint32_t modelID);
	//D3D12_GPU_VIRTUAL_ADDRESS BuildDynamicBLAS(const RayTracingInstanceDesc& inst, uint32_t instanceIndex);
	D3D12_GPU_VIRTUAL_ADDRESS BuildDynamicBLAS(const RayTracingInstanceDesc& inst, ID3D12Resource* keyResource);

	// DXCヘルパー
	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
		const std::wstring& filePath,
		const wchar_t* profile
	);

private:
	Fngine* engine_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Device5> device5_ = nullptr;Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> dxrCmdList_ = nullptr;
	UINT width_ = 1280;
	UINT height_ = 720;

	// パイプライン & SBT リソース
	Microsoft::WRL::ComPtr<ID3D12RootSignature> globalRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12StateObject> rtpso_;
	Microsoft::WRL::ComPtr<IDxcBlob> raytracingShaderBlob_;
	Microsoft::WRL::ComPtr<ID3D12Resource> sbtBuffer_;
	D3D12_DISPATCH_RAYS_DESC dispatchDesc_{};

	// 出力テクスチャ
	std::unique_ptr<RWTexture2D> outputTexture_;
	D3D12_GPU_DESCRIPTOR_HANDLE outputUAVHandleGPU_{};

	// カメラ
	std::unique_ptr<ConstantBuffer<RayTracingCamera>> cameraBuffer_;

	// 登録されたレイトレインスタンス群
	std::vector<RayTracingInstanceDesc> registeredInstances_;

	struct BLASResource {
		~BLASResource() {
			resultBuffer.Reset();
			scratchBuffer.Reset();
		}
		Microsoft::WRL::ComPtr<ID3D12Resource> resultBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> scratchBuffer;
	};
	std::unordered_map<uint32_t, BLASResource> staticBLASMap_;
	//std::vector<BLASResource> dynamicBLASPool_;
	std::unordered_map<ID3D12Resource*, BLASResource> dynamicBLASMap_;

	// TLAS 用リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> tlasResultBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> tlasScratchBuffer_;
	std::unique_ptr<Structured<D3D12_RAYTRACING_INSTANCE_DESC>> instanceDescBuffer_;

	std::unique_ptr<Structured<RayTracingInstanceMaterial>> materialStructuredBuffer_;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtDescriptorHeap_;
	UINT descriptorSize_ = 0;

	static constexpr UINT MAX_HAIR_BUFFERS = 64;
	// ヒープ全体のサイズ = Output UAV (1) + Depth SRV (1) + Hair SRVs (64) = 66
	static constexpr UINT TOTAL_DESCRIPTORS = 2 + MAX_HAIR_BUFFERS;
};