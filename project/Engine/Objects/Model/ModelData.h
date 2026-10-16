#pragma once
#include <string>
#include <vector>
#include <optional>
#include <map>
#include <span>
#include <wrl.h>
#include "VertexData.h"
#include "WorldTransform.h"
#include "ResourceFunc.h"
#include "Structured.h"
#include "Constant.h"
#include "Fngine.h"

struct MaterialData {
	std::string textureFilePath;
};

struct Node {
	WorldTransform transform;
	std::string name;
	std::vector<Node> children;
};

struct Joint {
	WorldTransform transform;// Transformの情報
	Matrix4x4 skeletonSpaceMatrix;// localMatrix
	std::string name;// skeletonSpaceでの変換行列
	std::vector<int32_t> children;// 名前
	int32_t index;// 自身のインデックス
	std::optional<int32_t> parent;// 親ジョイントのIndex
};

// --------------------------------------------------
// 骨格の静的テンプレート（名前・親子構造・初期姿勢）
// --------------------------------------------------
struct JointTemplate {
	std::string name;
	int32_t index;
	std::optional<int32_t> parent;
	std::vector<int32_t> children;
	Transform initialTransform;
};

struct SkeletonTemplate {
	int32_t rootIndex = -1;
	std::map<std::string, int32_t> jointMap;
	std::vector<JointTemplate> joints;
};

class Skeleton {
public:
	void CreateSkeleton(const Node& rootNode);
	void CreateFromTemplate(const SkeletonTemplate& skelTemplate);
	void Update();

	// --- ボーン行列の取得 ---

	/// <summary>
	/// ボーン名から指定したボーンの skeletonSpaceMatrix を取得する
	/// </summary>
	std::optional<Matrix4x4> GetJointMatrix(const std::string& jointName) const {
		auto it = jointMap_.find(jointName);
		if (it != jointMap_.end()) {
			return joints_[it->second].skeletonSpaceMatrix;
		}
		return std::nullopt; // ボーンが存在しない場合
	}

	/// <summary>
	/// インデックスから指定したボーンの skeletonSpaceMatrix を取得する（毎フレーム呼ぶなら高速）
	/// </summary>
	std::optional<Matrix4x4> GetJointMatrix(int32_t jointIndex) const {
		if (jointIndex >= 0 && jointIndex < static_cast<int32_t>(joints_.size())) {
			return joints_[jointIndex].skeletonSpaceMatrix;
		}
		return std::nullopt;
	}

	/// <summary>
	/// ボーン名からインデックスを取得する（事前にキャッシュしたい時用）
	/// </summary>
	int32_t GetJointIndex(const std::string& jointName) const {
		auto it = jointMap_.find(jointName);
		return (it != jointMap_.end()) ? it->second : -1;
	}

	size_t GetNumJoints() const { return joints_.size(); }

	const auto& GetJointMap() const {
		return jointMap_;
	}

	const Joint& GetJoint(int index) const {
		return joints_[index];
	}

private:
	int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints);
public:
	int32_t root_;// RootJointのIndex
	std::map<std::string, int32_t> jointMap_;// Joint名とIndexの辞書
	std::vector<Joint> joints_;// 所属しているジョイント
};

struct VertexWeightData {
	float weight;
	uint32_t vertexIndex;
};

struct JointWeightData {
	Matrix4x4 inverseBindPoseMatrix;
	std::vector<VertexWeightData> vertexWeights;
};

struct ModelData {
	std::map<std::string, JointWeightData> skinClusterData;
	std::vector<VertexData> vertices;
	std::vector<uint32_t> indices;
	MaterialData material;
	Node rootNode;
};

const uint32_t kNumMaxInfluence = 4;
struct VertexInfluence {
	std::array<float, kNumMaxInfluence>weights;
	std::array<int32_t, kNumMaxInfluence>jointIndices;
};

struct WellForGPU {
	// 位置用
	Matrix4x4 skeletonSpaceMatrix;
	// 法線用(ライトのやつ)
	Matrix4x4 skeletonSpaceInverseTransMatrix;
};

struct SkinningInformation {
	uint32_t numVertices;
};

// --------------------------------------------------
// スキニング計算用の静的データ（逆バインドポーズ & 頂点ウェイト）
// --------------------------------------------------
struct SkinningStaticData {
	std::vector<Matrix4x4> inverseBindPoseMatrices; // Jointごとの逆バインドポーズ行列
	std::vector<VertexInfluence> influences;        // 頂点ごとのウェイト・JointIndex情報 (t2用)
};

class ObjectData;

class SkinCluster {
public:
	void Create(Fngine* engine,const Skeleton& skeleton,const ModelData& modelData);
	void Create(Fngine* engine, const std::string& modelID, const ObjectData& objectData);
	void Update(const Skeleton& skeleton);
	void DispatchComputeShader(ID3D12GraphicsCommandList* commandList);
	uint32_t GetNumVertices() const { return numVertices_; }
	D3D12_VERTEX_BUFFER_VIEW GetOutputVertexBufferView() const {
		D3D12_VERTEX_BUFFER_VIEW vbView{};
		if (outputVertices_ && outputVertices_->GetResource()) {
			vbView.BufferLocation = outputVertices_->GetResource()->GetGPUVirtualAddress();
			vbView.SizeInBytes = sizeof(VertexData) * numVertices_;
			vbView.StrideInBytes = sizeof(VertexData);
		}
		return vbView;
	}
public:
	std::vector<Matrix4x4> inverseBindPoseMatrices_;
	uint32_t numVertices_ = 0;
	std::unique_ptr<Structured<WellForGPU>> palette_;             // t0
	std::unique_ptr<Structured<VertexData>> inputVertices_;       // t1
	std::unique_ptr<Structured<VertexInfluence>> influences_;     // t2
	std::unique_ptr<RWStructured<VertexData>> outputVertices_;    // u0
	std::unique_ptr<ConstantBuffer<SkinningInformation>> skinningInfo_; // b0
};