#pragma once
#include <memory>
#include "SceneType.h"

class Scene;

class SceneFactory {
public:
	/// <summary>
	/// シーン識別子から対応する Scene の unique_ptr を生成
	/// </summary>
	/// <param name="type"></param>
	/// <returns></returns>
	static std::unique_ptr<Scene> CreateScene(SceneType type);
};