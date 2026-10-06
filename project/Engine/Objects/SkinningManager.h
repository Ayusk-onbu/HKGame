#pragma once
#include "ModelData.h"
#include "ISingleton.h"
#include <unordered_map>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

class SkinningManager
	: public ISingleton<SkinningManager>
{
public:
	friend class ISingleton<SkinningManager>;

public:
    // --- IDベース登録関数 ---
    void RegisterFromAssimp(uint32_t id, const aiScene* scene, uint32_t numVertices);
    void RegisterFromModelData(
        uint32_t id,
        const Node& rootNode,
        const std::map<std::string, JointWeightData>& skinClusterData,
        uint32_t numVertices
    );

    /// <summary>
    /// AssimpのSceneからスキニング静的データを解析・一括登録する
    /// </summary>
    void RegisterFromAssimp(const std::string& id, const aiScene* scene, uint32_t numVertices);

    /// <summary>
    /// ModelData の解析結果からスキニング静的データを変換・登録する
    /// </summary>
    void RegisterFromModelData(
        const std::string& id,
        const Node& rootNode,
        const std::map<std::string, JointWeightData>& skinClusterData,
        uint32_t numVertices
    );

    // SkinCluster のポインタを登録
    void RegisterActiveSkinCluster(SkinCluster* skinCluster) {
        if (skinCluster) {
            activeSkinClusters_.push_back(skinCluster);
        }
    }

    // 登録された全 SkinCluster の CS を一括実行！
    void DispatchAll(ID3D12GraphicsCommandList* commandList) {
        for (auto* cluster : activeSkinClusters_) {
            cluster->DispatchComputeShader(commandList); // SkinCluster自身のDispatchを呼ぶ
        }
    }

    void ClearActiveSkinClusters() {
        activeSkinClusters_.clear();
    }

    // データ取得関数
    const SkeletonTemplate* GetSkeletonTemplate(uint32_t id) const;
    const SkinningStaticData* GetSkinningStaticData(uint32_t id) const;
    bool HasSkinningData(uint32_t id) const;

    const SkeletonTemplate* GetSkeletonTemplate(const std::string& id) const;
    const SkinningStaticData* GetSkinningStaticData(const std::string& id) const;
    bool HasSkinningData(const std::string& id) const;

private:
    // aiNode 階層から SkeletonTemplate を再帰構築する内部関数
    int32_t CreateJointTemplate(const aiNode* node, const std::optional<int32_t>& parent, SkeletonTemplate& outTemplate);
    int32_t CreateJointTemplateFromNode(const Node& node, const std::optional<int32_t>& parent, SkeletonTemplate& outTemplate);

    template <typename T>
    void EnsureCapacity(std::vector<T>& vec, uint32_t id) {
        if (id >= vec.size()) {
            vec.resize(id + 1);
        }
    }
private:
    std::vector<SkeletonTemplate> skeletonTemplates_;
    std::vector<SkinningStaticData> skinningStaticDatas_;
    std::vector<bool> hasSkinningDataFlags_;
    std::vector<SkinCluster*> activeSkinClusters_;
};