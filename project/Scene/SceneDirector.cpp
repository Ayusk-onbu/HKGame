#include "SceneDirector.h"
#include "CameraSystem.h"
#include "ModelManager.h"
#include "UIAnimation.h"
#include "MotionManager.h"

SceneDirector::~SceneDirector() {
	delete currentScene_;
}

void SceneDirector::Initialize(Scene& firstScene) {
	// Loadは何か他のクラスにまとめるべきなのではないのであろうか
	LoadModelData();
	LoadTexture();
	LoadMusic();
	UIHAnimationManager::GetInstance()->Load();
	MotionManager::GetInstance()->LoadMotions("resources/Data/Motion/Hermite/");

	// 最初のシーンの初期化処理
	currentScene_ = &firstScene;
	currentScene_->FngineSetUp(*p_fngine_);
	currentScene_->Initialize();
	currentScene_->GetSceneDirector(this);

	// 使うカメラの作成
	CameraSystem::GetInstance()->MakeCamera("NormalCamera", CameraType::Normal);
	CameraSystem::GetInstance()->MakeCamera("DebugCamera", CameraType::Debug);
	CameraSystem::GetInstance()->MakeCamera("GameCamera", CameraType::Game);
	CameraSystem::GetInstance()->SetActiveCamera("GameCamera");
}

void SceneDirector::Update() {
	// 更新処理

	// [ カメラ ]
	CameraSystem::GetInstance()->Update();
	// [ カメラの情報をGPUへ ]
	p_fngine_->GetCameraForGPU().Update(CameraSystem::GetInstance()->GetActiveCamera()->GetTranslation());

	bool pauseNow = PauseSystem::GetInstance()->Update(currentScene_->CanPause());

	// [ *** ポーズ中じゃない *** ]
	if (pauseNow == false) {
		// [ ゲーム ]
		if (isGameRunning_) {
			// [ シーン ]
			currentScene_->Update();
		}
	}
	// [ *** ポーズ中 *** ]
	else if (pauseNow == true) {
		currentScene_->PauseUpdate();
	}

	// [ ライト ] 
	p_fngine_->GetPointLight().Update();
	p_fngine_->GetSpotLight().Update();
	p_fngine_->GetAreaLight().Update();
}

void SceneDirector::Draw() {
	// 描画処理
	bool pauseNow = PauseSystem::GetInstance()->GetIsPause();
	
	// [ ポーズ中じゃない ]
	if (pauseNow == false) {
		// [ ゲーム ]
		if (isGameRunning_) {
			// [ シーン ]
			currentScene_->Draw();
		}
	}
	// [ ポーズ中 ]
	else if (pauseNow == true) {
		currentScene_->PauseDraw();
	}
}

void SceneDirector::ImGui() {
	ImGuiManager::GetInstance()->DrawSlider("DirectionalLight : pos", p_fngine_->GetLight().directionalLightData_->direction, -1.0f, 1.0f);
	ImGuiManager::GetInstance()->DrawSlider("DirectionalLight : color", p_fngine_->GetLight().directionalLightData_->color, 0.0f, 1.0f);
	PSOManager::GetInstance()->ImGui();
#ifdef USE_IMGUI
	if (ImGui::TreeNode("SceneDirector")) {

		if (ImGui::Button("TestScene")) {
			RequestChangeScene(new TestScene());
		}
		ImGui::SameLine();
		if (ImGui::Button("TitleScene")) {
			RequestChangeScene(new TitleScene());
		}
		ImGui::SameLine();
		if (ImGui::Button("GameScene")) {
			RequestChangeScene(new GameScene());
		}
		ImGui::SameLine();
		if (ImGui::Button("GameOverScene")) {
			RequestChangeScene(new GameOverScene());
		}
		ImGui::SameLine();
		if (ImGui::Button("GameClearScene")) {
			RequestChangeScene(new ClearScene());
		}

		ImGui::TreePop();
	}
#endif//USE_IMGUI
}

void SceneDirector::RequestChangeScene(Scene* newScene) {
	if (currentScene_) {
		// 今使っているsceneを削除
		delete currentScene_;
	}

	// そんなことは起きないと思うけど一応
	// ポーズが切れるようにする
	PauseSystem::GetInstance()->SetPause(false);

	// 新しいsceneを取得
	currentScene_ = newScene;
	currentScene_->FngineSetUp(*p_fngine_);
	currentScene_->Initialize();
	currentScene_->GetSceneDirector(this);
}

void SceneDirector::LoadModelData() {
	std::string name;
	name = ModelManager::GetInstance()->LoadObj("cube.obj","resources",LoadFileType::OBJ);
	name = ModelManager::GetInstance()->LoadObj("axis.obj", "resources", LoadFileType::OBJ);
	name = ModelManager::GetInstance()->LoadObj("AnimatedCube.gltf", "resources/AnimatedCube");
	name = ModelManager::GetInstance()->LoadObj("walk.gltf", "resources/Human");
	name = ModelManager::GetInstance()->LoadObj("sneakWalk.gltf", "resources/Human");
	name = ModelManager::GetInstance()->LoadObj("simpleSkin.gltf", "resources/simpleSkin");
	name = ModelManager::GetInstance()->LoadObj("ground.obj", "resources", LoadFileType::OBJ);
	name = ModelManager::GetInstance()->LoadObj("enemy.obj", "resources/Game", LoadFileType::OBJ);
	name = ModelManager::GetInstance()->LoadObj("player.obj", "resources/Game", LoadFileType::OBJ);
	name = ModelManager::GetInstance()->LoadObj("Confuse.obj", "resources/Star", LoadFileType::OBJ);
	name = ModelManager::GetInstance()->LoadObj("stone.obj", "resources/Game", LoadFileType::OBJ);
	name = ModelManager::GetInstance()->LoadObj("bullet.obj", "resources", LoadFileType::OBJ);
	name = ModelManager::GetInstance()->LoadObj("debugBlock.obj", "resources", LoadFileType::OBJ);
	name = ModelManager::GetInstance()->LoadObj("plane.gltf", "resources");
	name = ModelManager::GetInstance()->LoadObj("ulthimaSky.obj", "resources", LoadFileType::OBJ);
	name = ModelManager::GetInstance()->LoadObj("UmbrellaHandle.obj", "resources/Umbrella", LoadFileType::OBJ);
	name = ModelManager::GetInstance()->LoadObj("UmbrellaTop.obj", "resources/Umbrella", LoadFileType::OBJ);
	name = ModelManager::GetInstance()->LoadObj("UmbrellaTopClose.obj", "resources/Umbrella", LoadFileType::OBJ);
}

void SceneDirector::LoadTexture() {
	std::string name;
	name = TextureManager::GetInstance()->LoadTexture("GridLine.png", "resources");
	name = TextureManager::GetInstance()->LoadTexture("ulthimaSky.png","resources");
	name = TextureManager::GetInstance()->LoadTexture("Confuse.png","resources/Star");
	name = TextureManager::GetInstance()->LoadTexture("bullet.png", "resources");
	name = TextureManager::GetInstance()->LoadTexture("circle.png", "resources");
	name = TextureManager::GetInstance()->LoadTexture("Legends_Ground.png", "resources");
	name = TextureManager::GetInstance()->LoadTexture("GameClear.png", "resources");
	name = TextureManager::GetInstance()->LoadTexture("GameOver.png", "resources");
	name = TextureManager::GetInstance()->LoadTexture("Purpose.png", "resources/Title");
	name = TextureManager::GetInstance()->LoadTexture("UI.png", "resources");
	name = TextureManager::GetInstance()->LoadTexture("TitleBack.png", "resources/Title");
	name = TextureManager::GetInstance()->LoadTexture("Title.png", "resources/Title");
	name = TextureManager::GetInstance()->LoadTexture("monsterBall.png", "resources");
	name = TextureManager::GetInstance()->LoadTexture("PlayGuide.png", "resources/Game");
}

void SceneDirector::LoadMusic() {
	std::string name;
	name = Music::GetInstance()->LoadSE("loop101204.wav", "resources");
}