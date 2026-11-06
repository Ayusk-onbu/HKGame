#include "SceneFactory.h"
#include "Scene.h"

#include "TitleScene.h"
#include "GameScene.h"
#include "TestScene.h"
#include "GameOver.h"
#include "ClearScene.h"
#include <cassert>

std::unique_ptr<Scene> SceneFactory::CreateScene(SceneType type) {
	switch (type) {
	case SceneType::Title:    return std::make_unique<TitleScene>();
	case SceneType::Game:     return std::make_unique<GameScene>();
	case SceneType::Test:     return std::make_unique<TestScene>();
	case SceneType::GameOver: return std::make_unique<GameOverScene>();
	case SceneType::Clear:    return std::make_unique<ClearScene>();
	default:
		assert(false && "指定された SceneType が存在しません。");
		return nullptr;
	}
}