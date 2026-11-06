#pragma once
#include "SceneType.h"
#include <memory>

class Fngine;
class Scene;

class SceneDirector
{
public:
	SceneDirector();
	~SceneDirector();

	// コピー・ムーブ禁止
	SceneDirector(const SceneDirector&) = delete;
	SceneDirector& operator=(const SceneDirector&) = delete;
public:
	// 初期化処理
	void Initialize(SceneType firstSceneType);
	// 更新・描画
	void Update();
	void Draw();
	void ImGui();

	// 遅延シーン遷移リクエスト(Enum)
	void RequestChangeScene(SceneType newSceneType);
	// 遅延シーン遷移リクエスト(インスタンス)
	void RequestChangeScene(std::unique_ptr<Scene> newScene);
public:
	void SetUpFngine(Fngine& fngine) { p_fngine_ = &fngine; }
private:
	// 安全なフレームで呼び出される内部遷移処理
	void ProcessPendingSceneChange();
	
	void LoadModelData();
	void LoadTexture();
	void LoadMusic();
private:
	Fngine* p_fngine_;
	std::unique_ptr<Scene> currentScene_ = nullptr;
	std::unique_ptr<Scene> nextScene_ = nullptr;
	bool isGameRunning_ = true;
};

// Load関係を分ける必要ありそう
