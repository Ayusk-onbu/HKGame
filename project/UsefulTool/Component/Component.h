#pragma once
#include "Structures.h"
#include "FileSystem.h"

class DynamicObject;

class Component {
public:
	DynamicObject* master_ = nullptr;

	virtual ~Component() = default;
	virtual void Initialize() {}
	virtual void Update(float deltaTime){}
	virtual void Draw() {}
	// 衝突が起きたときに呼ぶ関数
	virtual void OnCollision(DynamicObject* other, const Vector3& pushOut) {}
	// エディタ編集用
	virtual void DrawUI() {}
	// コンポーネントの種類名
	virtual std::string GetComponentName()const = 0;
	// jsonに値を保存
	virtual json Serialize()const = 0;
	// jsonから値を取得
	virtual void Deserialize(const json& j) = 0;
};