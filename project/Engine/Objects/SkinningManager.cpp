#include "SkinningManager.h"
#include "Log.h"
#include "MathUtils.h"
#include "StringPool.h"

void SkinningManager::RegisterFromAssimp(uint32_t id, const aiScene* scene, uint32_t numVertices) {
    if (!scene || !scene->HasMeshes()) return;

    SkeletonTemplate skelTemplate;
    SkinningStaticData staticData;

    if (scene->mRootNode) {
        skelTemplate.rootIndex = CreateJointTemplate(scene->mRootNode, std::nullopt, skelTemplate);
    }

    uint32_t numJoints = static_cast<uint32_t>(skelTemplate.joints.size());
    staticData.inverseBindPoseMatrices.resize(numJoints, Matrix4x4::Make::Identity());
    staticData.influences.resize(numVertices, VertexInfluence{});

    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        aiMesh* mesh = scene->mMeshes[meshIndex];

        for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
            aiBone* bone = mesh->mBones[boneIndex];
            std::string jointName = bone->mName.C_Str();

            auto it = skelTemplate.jointMap.find(jointName);
            if (it == skelTemplate.jointMap.end()) continue;

            uint32_t targetJointIdx = it->second;

            aiMatrix4x4 bindPoseAssimp = bone->mOffsetMatrix.Inverse();
            aiVector3D scale, translate;
            aiQuaternion rotate;
            bindPoseAssimp.Decompose(scale, rotate, translate);

            Matrix4x4 bindPose = MakeAffineMatrix(
                { scale.x, scale.y, scale.z },
                { rotate.x, -rotate.y, -rotate.z, rotate.w },
                { -translate.x, translate.y, translate.z }
            );
            staticData.inverseBindPoseMatrices[targetJointIdx] = Matrix4x4::Inverse(bindPose);

            for (uint32_t weightIdx = 0; weightIdx < bone->mNumWeights; ++weightIdx) {
                uint32_t vIdx = bone->mWeights[weightIdx].mVertexId;
                float weight = bone->mWeights[weightIdx].mWeight;

                if (vIdx >= numVertices) continue;

                auto& currentInfluence = staticData.influences[vIdx];
                for (uint32_t i = 0; i < kNumMaxInfluence; ++i) {
                    if (currentInfluence.weights[i] == 0.0f) {
                        currentInfluence.weights[i] = weight;
                        currentInfluence.jointIndices[i] = targetJointIdx;
                        break;
                    }
                }
            }
        }
    }

    EnsureCapacity(skeletonTemplates_, id);
    EnsureCapacity(skinningStaticDatas_, id);
    EnsureCapacity(hasSkinningDataFlags_, id);

    skeletonTemplates_[id] = std::move(skelTemplate);
    skinningStaticDatas_[id] = std::move(staticData);
    hasSkinningDataFlags_[id] = true;

    Log::View("Registered Skinning Data for ID: " + std::to_string(id));
}

void SkinningManager::RegisterFromAssimp(const std::string& idStr, const aiScene* scene, uint32_t numVertices) {
    RegisterFromAssimp(StringPool::GetInstance()->GetStringID(idStr), scene, numVertices);
}

void SkinningManager::RegisterFromModelData(
    uint32_t id,
    const Node& rootNode,
    const std::map<std::string, JointWeightData>& skinClusterData,
    uint32_t numVertices)
{
    SkeletonTemplate skelTemplate;
    SkinningStaticData staticData;

    skelTemplate.rootIndex = CreateJointTemplateFromNode(rootNode, std::nullopt, skelTemplate);

    uint32_t numJoints = static_cast<uint32_t>(skelTemplate.joints.size());
    staticData.inverseBindPoseMatrices.resize(numJoints, Matrix4x4::Make::Identity());
    staticData.influences.resize(numVertices, VertexInfluence{});

    for (const auto& [jointName, weightData] : skinClusterData) {
        auto it = skelTemplate.jointMap.find(jointName);
        if (it == skelTemplate.jointMap.end()) continue;

        uint32_t targetJointIdx = it->second;
        staticData.inverseBindPoseMatrices[targetJointIdx] = weightData.inverseBindPoseMatrix;

        for (const auto& vWeight : weightData.vertexWeights) {
            if (vWeight.vertexIndex >= numVertices) continue;

            auto& currentInfluence = staticData.influences[vWeight.vertexIndex];
            for (uint32_t i = 0; i < kNumMaxInfluence; ++i) {
                if (currentInfluence.weights[i] == 0.0f) {
                    currentInfluence.weights[i] = vWeight.weight;
                    currentInfluence.jointIndices[i] = targetJointIdx;
                    break;
                }
            }
        }
    }

    EnsureCapacity(skeletonTemplates_, id);
    EnsureCapacity(skinningStaticDatas_, id);
    EnsureCapacity(hasSkinningDataFlags_, id);

    skeletonTemplates_[id] = std::move(skelTemplate);
    skinningStaticDatas_[id] = std::move(staticData);
    hasSkinningDataFlags_[id] = true;

    Log::View("Registered Skinning Data for ID: " + std::to_string(id));
}

void SkinningManager::RegisterFromModelData(
    const std::string& idStr,
    const Node& rootNode,
    const std::map<std::string, JointWeightData>& skinClusterData,
    uint32_t numVertices)
{
    RegisterFromModelData(StringPool::GetInstance()->GetStringID(idStr), rootNode, skinClusterData, numVertices);
}

int32_t SkinningManager::CreateJointTemplateFromNode(const Node& node, const std::optional<int32_t>& parent, SkeletonTemplate& outTemplate) {
    JointTemplate joint;
    joint.name = node.name;
    joint.index = static_cast<int32_t>(outTemplate.joints.size());
    joint.parent = parent;
    joint.initialTransform = node.transform.transform_;

    outTemplate.joints.push_back(joint);
    outTemplate.jointMap[joint.name] = joint.index;

    for (const Node& child : node.children) {
        int32_t childIdx = CreateJointTemplateFromNode(child, joint.index, outTemplate);
        outTemplate.joints[joint.index].children.push_back(childIdx);
    }

    return joint.index;
}

int32_t SkinningManager::CreateJointTemplate(const aiNode* node, const std::optional<int32_t>& parent, SkeletonTemplate& outTemplate) {
    JointTemplate joint;
    joint.name = node->mName.C_Str();

    aiVector3D scale, translate;
    aiQuaternion rotate;
    node->mTransformation.Decompose(scale, rotate, translate);

    joint.initialTransform.Initialize();
    joint.initialTransform.scale_ = { scale.x, scale.y, scale.z };
    joint.initialTransform.translation_ = { -translate.x, translate.y, translate.z };

    joint.index = static_cast<int32_t>(outTemplate.joints.size());
    joint.parent = parent;

    outTemplate.joints.push_back(joint);
    outTemplate.jointMap[joint.name] = joint.index;

    for (uint32_t i = 0; i < node->mNumChildren; ++i) {
        int32_t childIndex = CreateJointTemplate(node->mChildren[i], joint.index, outTemplate);
        outTemplate.joints[joint.index].children.push_back(childIndex);
    }

    return joint.index;
}

const SkeletonTemplate* SkinningManager::GetSkeletonTemplate(uint32_t id) const {
    if (id < skeletonTemplates_.size() && HasSkinningData(id)) {
        return &skeletonTemplates_[id];
    }
    return nullptr;
}

const SkinningStaticData* SkinningManager::GetSkinningStaticData(uint32_t id) const {
    if (id < skinningStaticDatas_.size() && HasSkinningData(id)) {
        return &skinningStaticDatas_[id];
    }
    return nullptr;
}

bool SkinningManager::HasSkinningData(uint32_t id) const {
    return (id < hasSkinningDataFlags_.size()) && hasSkinningDataFlags_[id];
}

const SkeletonTemplate* SkinningManager::GetSkeletonTemplate(const std::string& idStr) const {
    return GetSkeletonTemplate(StringPool::GetInstance()->GetStringID(idStr));
}
const SkinningStaticData* SkinningManager::GetSkinningStaticData(const std::string& idStr) const {
    return GetSkinningStaticData(StringPool::GetInstance()->GetStringID(idStr));
}
bool SkinningManager::HasSkinningData(const std::string& idStr) const {
    return HasSkinningData(StringPool::GetInstance()->GetStringID(idStr));
}

//void SkinningManager::RegisterFromAssimp(const std::string& id, const aiScene* scene, uint32_t numVertices) {
//    if (!scene || !scene->HasMeshes()) return;
//
//    SkeletonTemplate skelTemplate;
//    SkinningStaticData staticData;
//
//    // 1. ノード階層構造の構築 (SkeletonTemplate)
//    if (scene->mRootNode) {
//        skelTemplate.rootIndex = CreateJointTemplate(scene->mRootNode, std::nullopt, skelTemplate);
//    }
//
//    // 2. スキニングデータ領域の確保
//    uint32_t numJoints = static_cast<uint32_t>(skelTemplate.joints.size());
//    staticData.inverseBindPoseMatrices.resize(numJoints, Matrix4x4::Make::Identity());
//    staticData.influences.resize(numVertices, VertexInfluence{});
//
//    // 3. ボーンウェイト & InverseBindPose行列の抽出
//    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
//        aiMesh* mesh = scene->mMeshes[meshIndex];
//
//        for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
//            aiBone* bone = mesh->mBones[boneIndex];
//            std::string jointName = bone->mName.C_Str();
//
//            auto it = skelTemplate.jointMap.find(jointName);
//            if (it == skelTemplate.jointMap.end()) continue;
//
//            uint32_t targetJointIdx = it->second;
//
//            // InverseBindPoseMatrix の計算
//            aiMatrix4x4 bindPoseAssimp = bone->mOffsetMatrix.Inverse();
//            aiVector3D scale, translate;
//            aiQuaternion rotate;
//            bindPoseAssimp.Decompose(scale, rotate, translate);
//
//            Matrix4x4 bindPose = MakeAffineMatrix(
//                { scale.x, scale.y, scale.z },
//                { rotate.x, -rotate.y, -rotate.z, rotate.w },
//                { -translate.x, translate.y, translate.z }
//            );
//            staticData.inverseBindPoseMatrices[targetJointIdx] = Matrix4x4::Inverse(bindPose);
//
//            // 各頂点への Influence (ボーンIndex と ウェイト) の登録
//            for (uint32_t weightIdx = 0; weightIdx < bone->mNumWeights; ++weightIdx) {
//                uint32_t vIdx = bone->mWeights[weightIdx].mVertexId;
//                float weight = bone->mWeights[weightIdx].mWeight;
//
//                if (vIdx >= numVertices) continue;
//
//                auto& currentInfluence = staticData.influences[vIdx];
//                for (uint32_t i = 0; i < kNumMaxInfluence; ++i) {
//                    if (currentInfluence.weights[i] == 0.0f) {
//                        currentInfluence.weights[i] = weight;
//                        currentInfluence.jointIndices[i] = targetJointIdx;
//                        break;
//                    }
//                }
//            }
//        }
//    }
//
//    // 4. マネージャーの辞書に格納
//    skeletonTemplates_[id] = std::move(skelTemplate);
//    skinningStaticDatas_[id] = std::move(staticData);
//
//    Log::View("Registered Skinning Data for ID: " + id);
//}
//
//void SkinningManager::RegisterFromModelData(
//    const std::string& id,
//    const Node& rootNode,
//    const std::map<std::string, JointWeightData>& skinClusterData,
//    uint32_t numVertices)
//{
//    SkeletonTemplate skelTemplate;
//    SkinningStaticData staticData;
//
//    // 1. Node 階層構造から SkeletonTemplate を生成
//    skelTemplate.rootIndex = CreateJointTemplateFromNode(rootNode, std::nullopt, skelTemplate);
//
//    uint32_t numJoints = static_cast<uint32_t>(skelTemplate.joints.size());
//    staticData.inverseBindPoseMatrices.resize(numJoints, Matrix4x4::Make::Identity());
//    staticData.influences.resize(numVertices, VertexInfluence{});
//
//    // 2. JointWeightData から InverseBindPoseMatrix と VertexInfluence(t2用) を抽出・変換
//    for (const auto& [jointName, weightData] : skinClusterData) {
//        auto it = skelTemplate.jointMap.find(jointName);
//        if (it == skelTemplate.jointMap.end()) continue;
//
//        uint32_t targetJointIdx = it->second;
//
//        // 逆バインドポーズ行列の格納
//        staticData.inverseBindPoseMatrices[targetJointIdx] = weightData.inverseBindPoseMatrix;
//
//        // ボーン視点(JointWeightData) から 頂点視点(VertexInfluence) への変換
//        for (const auto& vWeight : weightData.vertexWeights) {
//            if (vWeight.vertexIndex >= numVertices) continue;
//
//            auto& currentInfluence = staticData.influences[vWeight.vertexIndex];
//            for (uint32_t i = 0; i < kNumMaxInfluence; ++i) {
//                if (currentInfluence.weights[i] == 0.0f) {
//                    currentInfluence.weights[i] = vWeight.weight;
//                    currentInfluence.jointIndices[i] = targetJointIdx;
//                    break;
//                }
//            }
//        }
//    }
//
//    // 3. マネージャーの辞書へ登録
//    skeletonTemplates_[id] = std::move(skelTemplate);
//    skinningStaticDatas_[id] = std::move(staticData);
//
//    Log::View("Registered Skinning Data for ID: " + id);
//}
//
//int32_t SkinningManager::CreateJointTemplateFromNode(const Node& node, const std::optional<int32_t>& parent, SkeletonTemplate& outTemplate) {
//    JointTemplate joint;
//    joint.name = node.name;
//    joint.index = static_cast<int32_t>(outTemplate.joints.size());
//    joint.parent = parent;
//
//    // Node の Transform(S, R, T) を初期姿勢としてコピー
//    joint.initialTransform = node.transform.transform_;
//
//    outTemplate.joints.push_back(joint);
//    outTemplate.jointMap[joint.name] = joint.index;
//
//    for (const Node& child : node.children) {
//        int32_t childIdx = CreateJointTemplateFromNode(child, joint.index, outTemplate);
//        outTemplate.joints[joint.index].children.push_back(childIdx);
//    }
//
//    return joint.index;
//}
//
//int32_t SkinningManager::CreateJointTemplate(const aiNode* node, const std::optional<int32_t>& parent, SkeletonTemplate& outTemplate) {
//    JointTemplate joint;
//    joint.name = node->mName.C_Str();
//
//    aiVector3D scale, translate;
//    aiQuaternion rotate;
//    node->mTransformation.Decompose(scale, rotate, translate);
//
//    joint.initialTransform.Initialize();
//    joint.initialTransform.scale_ = { scale.x, scale.y, scale.z };
//    joint.initialTransform.translation_ = { -translate.x, translate.y, translate.z };
//    // クォータニオン保持等の設定...
//
//    joint.index = static_cast<int32_t>(outTemplate.joints.size());
//    joint.parent = parent;
//
//    outTemplate.joints.push_back(joint);
//    outTemplate.jointMap[joint.name] = joint.index;
//
//    for (uint32_t i = 0; i < node->mNumChildren; ++i) {
//        int32_t childIndex = CreateJointTemplate(node->mChildren[i], joint.index, outTemplate);
//        outTemplate.joints[joint.index].children.push_back(childIndex);
//    }
//
//    return joint.index;
//}
//
//const SkeletonTemplate* SkinningManager::GetSkeletonTemplate(const std::string& id) const {
//    auto it = skeletonTemplates_.find(id);
//    return (it != skeletonTemplates_.end()) ? &it->second : nullptr;
//}
//
//const SkinningStaticData* SkinningManager::GetSkinningStaticData(const std::string& id) const {
//    auto it = skinningStaticDatas_.find(id);
//    return (it != skinningStaticDatas_.end()) ? &it->second : nullptr;
//}
//
//bool SkinningManager::HasSkinningData(const std::string& id) const {
//    return skeletonTemplates_.find(id) != skeletonTemplates_.end();
//}