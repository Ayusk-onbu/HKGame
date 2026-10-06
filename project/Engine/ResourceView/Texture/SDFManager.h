#pragma once
#include "Structured.h"
#include "ModelManager.h"

// この構造体はいらないかも
struct SDFGPUParam {
    Vector3 gridMin;
    uint32_t textureIndex; // Bindless Texture3D Index
    Vector3 gridMax;
    float pad;
};

struct SDFData {
    RWTexture3D texture;
    Vector3 gridMin;
    Vector3 gridMax;
    uint32_t resolution;

    // SRV インデックス取得ヘルパー
    uint32_t GetSRVIndex() const { return texture.GetSrvIndex(); }

    // GPU送信用パラメータを取得
    SDFGPUParam GetGPUParam() const {
        return SDFGPUParam{
            gridMin,
            texture.GetSrvIndex(),
            gridMax,
            0.0f
        };
    }
};

struct SDFBakeConfig
{
    Vector3 gridMin;     // SDFバウンディングボックスの最小点
    float  pad0;
    Vector3 gridMax;     // SDFバウンディングボックスの最大点
    float  pad1;
    uint32_t resolution[3];  // テクスチャ解像度 (例: 64, 64, 64)
    uint32_t totalNodes;
};

struct SDFInstanceGPU {
    Matrix4x4 worldInverse;// レイ変換用
    Vector3 gridMin;
    uint32_t textureIndex;
    Vector3 gridMax;
    float padding;
};

struct SDFInstanceData {
    uint32_t modelID;
    Matrix4x4 worldMatrix;
};

class SDFManager : public ISingleton<SDFManager>
{
    friend class ISingleton<SDFManager>;
public:
    void Initialize(Fngine* fngine);

    /// <summary>
    /// ModelIDだけ渡せば、ModelManagerからBVHと頂点を勝手に取得してSDFをベイクする！
    /// </summary>
    bool LoadAndBake(
        ID3D12GraphicsCommandList* commandList,
        const std::string& modelID,
        uint32_t resolution = 64
    );

    /// <summary>
    /// Int型に互換ModelIDだけ渡せば、ModelManagerからBVHと頂点を勝手に取得してSDFをベイクする！
    /// </summary>
    bool LoadAndBake(
        ID3D12GraphicsCommandList* commandList,
        uint32_t modelID,
        uint32_t resolution = 64
    );

    /// <summary>
    /// ModelManagerに登録されているモデルをすべてBakeする
    /// </summary>
    /// <param name="commandList"></param>
    /// <param name="resolution"></param>
    void BakeAll(ID3D12GraphicsCommandList* commandList, uint32_t resolution = 64);

private:
    /// <summary>
    /// モデルのBVHからSDFをベイクして管理下に登録する
    /// </summary>
    bool BakeAndRegister(
        ID3D12GraphicsCommandList* commandList,
        const std::string& modelID,
        const BVH* bvh,
        const Vector3& modelMinBounds,
        const Vector3& modelMaxBounds,
        uint32_t resolution = 64
    );

    /// <summary>
    /// モデルのBVHからSDFをベイクして管理下に登録する
    /// </summary>
    bool BakeAndRegister(
        ID3D12GraphicsCommandList* commandList,
        uint32_t modelID,
        const BVH* bvh,
        const Vector3& modelMinBounds,
        const Vector3& modelMaxBounds,
        uint32_t resolution = 64
    );

public:
    void ExportToDDS(
        ID3D12GraphicsCommandList* commandList,
        ID3D12CommandQueue* commandQueue, // GPU完了を待つために使用
        const std::string& modelID,
        const std::wstring& outputFilePath);

    /// <summary>
    /// 指定したモデルのSDFデータを取得する
    /// </summary>
    const SDFData* GetSDF(const std::string& modelID) const;
    const SDFData* GetSDF(uint32_t modelID) const;

private:
    Fngine* pFngine_ = nullptr;

    template <typename T>
    void EnsureCapacity(std::vector<T>& vec, uint32_t id) {
        if (id >= vec.size()) {
            vec.resize(id + 1);
        }
    }

    struct TempBakeResource {
        std::unique_ptr<Structured<GPUBVHNode>> nodeBuffer;
        std::unique_ptr<Structured<GPUPolygon>> polygonBuffer;
        std::unique_ptr<ConstantBuffer<SDFBakeConfig>> configBuffer;
    };
public:
    // IDごとのSDFデータ構造体マップ
    //std::unordered_map<std::string, std::unique_ptr<SDFData>> sdfMap_;
    std::vector<std::unique_ptr<SDFData>> sdfList_;
    // ベイク完了（GPU同期完了）まで一時バッファの寿命を保持するリスト
    // 怖いから5フレーム経ったら解放にしようかな
    std::vector<TempBakeResource> tempBakeResources_;

//
// 【 InstanceData関係 】
//
public:

    /// <summary>
    /// 毎フレーム全インスタンスをクリアする
    /// </summary>
    void ClearInstances();

    /// <summary>
    /// SDF影を描画したいオブジェクトのインスタンスを追加
    /// </summary>
    /// <param name="modelID">描画に使用しているModelのID</param>
    void AddInstance(uint32_t modelID, const Matrix4x4& worldMatrix);

    /// <summary>
    /// 収集したデータをGPUバッファに転送
    /// </summary>
    void UpdateGPUBuffer();

    /// <summary>
    /// SRV ハンドル
    /// </summary>
    D3D12_GPU_DESCRIPTOR_HANDLE GetInstanceBufferSRVHandleGPU() const {
        return instanceBuffer_ ? instanceBuffer_->GetSRVHandleGPU() : D3D12_GPU_DESCRIPTOR_HANDLE{};
    }

    /// <summary>
    /// 現在登録されているアクティブな SDF インスタンス数を取得
    /// </summary>
    uint32_t GetActiveInstanceCount() const {
        return static_cast<uint32_t>(activeInstances_.size());
    }

private:
    // 毎フレーム収集用リスト
    std::vector<SDFInstanceData> activeInstances_;

    // GPU一括送信用バッファ
    std::unique_ptr<Structured<SDFInstanceGPU>> instanceBuffer_;
    uint32_t maxInstanceCapacity_ = 0;
};

/*
* SDFはワールドに配置したいよね
* 使うIndexと使うやつのWorldMatrixほしいね
* てことはDraw前とかに登録して、なんかでまとめて置くのがいいね
*/