#pragma once
#include "Structures.h"

enum class SDFFunctionStackNodeType : uint32_t {
	// 形状ノード(0 ～ 99)
	Sphere = 0,
	Box = 1,
	Cylinder = 2,
	Torus = 3,

	// 演算ノード
	OpUnion = 100, // 和集合 (a ∪ b)
	OpSubtract = 101, // 差集合 (a - b)
	OpIntersection = 102, // 積集合 (a ∩ b)
	OpSmoothUnion = 103, // スムーズ結合
	OpSmoothSubtract = 104, // スムーズくり抜き
};

// 全体を対象のObjectのMatrixを使用する

	// --- 静的な場合 ---
	// 座標はほしい。回転もほしい。Scaleもほしい。
	// どのような形状もしくは演算なのかが欲しい
	// 形状の場合はそのサイズほしい。
	// 結合の場合は結合の幅

	// --- 動的に対応する場合 ---
	// 追従させる対象のMatrixの情報がほしい？
	// Skinningは物によって異なるからIDとかは無理
//  CPU側の構造体
struct SDFFunctionStackNodeCPU {
	SDFFunctionStackNodeType type = SDFFunctionStackNodeType::Sphere; // ノードの種類
	Vector3 params = { 1.0f,1.0f,1.0f };// 半径とか
	float smoothK = 0.0f; // スムーズ結合の幅 k
	// 相対的なオフセット
	Vector3 scale = { 1.0f,1.0f,1.0f }; // スケール
	Vector3 rotation = { 0.0f,0.0f,0.0f }; // 回転
	Vector3 position = { 0.0f,0.0f,0.0f }; // 平行移動

	std::string followJointName = "";
	const Matrix4x4* pFollowMatrix = nullptr; // 追従させる対象のMatrixの情報
};

// GPUに送るデータ：Node 1つ分の構造体
struct SDFFunctionStackNodeGPU {
	Matrix4x4 localToNodeInverse; // 形状の場合：ノードローカル変換の逆行列
	Vector3   params;             // 形状サイズ (xyz)
	uint32_t  nodeType;           // SDFStackNodeType

	float     smoothK;            // スムーズ結合の幅 k
	Vector3   padding;
};

// GPUに送るデータ：Stack全体の構造体 : 理解できていない
struct SDFFunctionStackInstanceGPU {
	Matrix4x4 worldInverse;    // ワールド → コンポーネントローカルの逆行列
	Vector3   aabbMin;         // ローカル AABB Min
	uint32_t  nodeStartOffset; // ノードバッファ内の開始位置
	Vector3   aabbMax;         // ローカル AABB Max
	uint32_t  nodeCount;       // このインスタンスに含まれる全ノード数
};