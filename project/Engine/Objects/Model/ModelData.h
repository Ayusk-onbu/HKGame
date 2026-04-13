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

class Skeleton {
public:
	void CreateSkeleton(const Node& rootNode);
	void Update();
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

class SkinCluster {
public:
	void Create(Fngine* engine,const Skeleton& skeleton,const ModelData& modelData);
	void Update(const Skeleton& skeleton);
	void DispatchComputeShader(ID3D12GraphicsCommandList* commandList);
	uint32_t GetNumVertices() const { return numVertices_; }
public:
	std::vector<Matrix4x4> inverseBindPoseMatrices_;
	uint32_t numVertices_ = 0;
	std::unique_ptr<Structured<WellForGPU>> palette_;             // t0
	std::unique_ptr<Structured<VertexData>> inputVertices_;       // t1
	std::unique_ptr<Structured<VertexInfluence>> influences_;     // t2
	std::unique_ptr<RWStructured<VertexData>> outputVertices_;    // u0
	std::unique_ptr<ConstantBuffer<SkinningInformation>> skinningInfo_; // b0
};