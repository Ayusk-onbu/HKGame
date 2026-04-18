#pragma once
#include "Structures.h"

class AABB {
public:
	// 中心からAABBを作成する※カス過ぎる設計で草
	static AABB World2AABB(Vector3 world);

	// AABBとAABBの当たり判定
	static bool IsHitAABB2AABB(const AABB& a, const AABB& b);

	// AABBの中心を取得
	Vector3 center()const;

	// 初期化 いらん
	void Initialize();

	/// <summary>
	/// Pointも含むようにAABBを拡張する
	/// </summary>
	/// <param name="point">新しいポイント</param>
	void Expand(const Vector3& point);

	/// <summary>
	/// 別のAABBも含むようにAABBを拡張する
	/// </summary>
	/// <param name="other">他のAABB</param>
	void Expand(const AABB& other);

	// 箱の各軸の長さを取得
	Vector3 GetSize()const;

	// AABBを極小の状態（無限大の逆）で初期化する（Expandのスタート地点用）
	void ResetToExtreme();
public:
	Vector3 min;
	Vector3 max;
};