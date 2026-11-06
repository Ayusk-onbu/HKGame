#include "SceneDirector.h"
#include "SceneFactory.h"
#include "CameraSystem.h"
#include "LightingSystem.h"
#include "ModelManager.h"
#include "UIHAnimation.h"
#include "Chronos.h"
#include "DrawManager.h"
#include "SDFManager.h"
#include "RayTracingManager.h"
#include "AnimationManager.h"
#include "SkinningManager.h"
#include "../Engine/Objects/Primitive/Box/PrimitiveBox.h"
#include "../Engine/Objects/Primitive/Ring/Ring.h"
#include "../Engine/Objects/Primitive/MagicPlane.h"
#include "../UsefulTool/EditorManager/Hair/HairEditor.h"
#include "../UsefulTool/EditorManager/Map/SceneEditor.h"
#include "../UsefulTool/EditorManager/Hermite/HermiteEditor.h"
#include "../UsefulTool/Component/ComponentFactory.h"
#include "Scene.h"
#include <pix3.h>

import MotionManager;

SceneDirector::SceneDirector() = default;
SceneDirector::~SceneDirector() = default;

void SceneDirector::Initialize(SceneType firstSceneType) {
	// Loadは何か他のクラスにまとめるべきなのではないのであろうか
	LoadTexture();
	LoadModelData();
	LoadMusic();

	UIHAnimationManager::GetInstance()->Load();
	MotionManager::GetInstance()->LoadMotions("resources/Data/Motion/Hermite/");

	SDFManager::GetInstance()->Initialize(p_fngine_);

	SDFManager::GetInstance()->BakeAll(p_fngine_->GetCommand().GetList().GetList().Get(),128);

	ComponentFactory::GetInstance()->Initialize();

	LightingSystem::GetInstance()->Initialize(p_fngine_);

	// 最初のシーンの初期化処理
	RequestChangeScene(firstSceneType);
	ProcessPendingSceneChange();

	// 使うカメラの作成
	CameraSystem::GetInstance()->MakeCamera("NormalCamera", CameraType::Normal);
	CameraSystem::GetInstance()->MakeCamera("DebugCamera", CameraType::Debug);
	CameraSystem::GetInstance()->MakeCamera("GameCamera", CameraType::Game);
	CameraSystem::GetInstance()->SetActiveCamera("DebugCamera");

	CameraSystem::GetInstance()->GetActiveCamera()->SetTheta(20.0f);
	CameraSystem::GetInstance()->GetActiveCamera()->SetRadius(20.0f);

	DrawManager::GetInstance()->Initialize(p_fngine_);

	auto editorMgr = EditorManager::GetInstance();
	auto* hermiteEditor = editorMgr->CreateEditor<HermiteEditor>();
	editorMgr->SetActiveEditor(hermiteEditor);
}

void SceneDirector::Update() {
	// 更新処理
	// 予約されているシーン遷移があれば、フレーム冒頭で安全に切り替える
	ProcessPendingSceneChange();

	if (!currentScene_)return;

	// [ カメラ ]
	CameraSystem::GetInstance()->Update();
	// [ カメラの情報をGPUへ ]
	p_fngine_->GetCameraForGPU().Update(CameraSystem::GetInstance()->GetActiveCamera()->GetTranslation());

	TimeKeeper::GetInstance()->Update(1.0f / 60.0f);

	// アクティブなSDFを削除
	SDFManager::GetInstance()->ClearInstances();

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
	if (!currentScene_)return;
	// 描画処理
	bool pauseNow = PauseSystem::GetInstance()->GetIsPause();
	
	// [ ポーズ中じゃない ]
	if (pauseNow == false) {
		// [ ゲーム ]
		if (isGameRunning_) {
			// [ シーン ]

		// =========================================================
		// 【Compute Shader パス】全 SkinCluster を一括 Dispatch！
		// =========================================================
			//※ リストの物体全てのMatrixをInverseしているためくそくそ重い
			SDFManager::GetInstance()->UpdateGPUBuffer();

			SkinningManager::GetInstance()->DispatchAll(p_fngine_->GetCommand().GetList().GetList().Get());

			// 【 DepthだけのPre描画 】
			currentScene_->DrawDepthPrePass();
			DrawManager::GetInstance()->Draw(RenderPass::DepthPrePass);

			// 【 メイン描画 】
			currentScene_->Draw();
			DrawManager::GetInstance()->Draw(RenderPass::MainColor);

			SkinningManager::GetInstance()->ClearActiveSkinClusters();
		}
	}
	// [ ポーズ中 ]
	else if (pauseNow == true) {
		currentScene_->PauseDraw();
		DrawManager::GetInstance()->Draw(RenderPass::MainColor);
	}
}

void SceneDirector::RequestChangeScene(SceneType nextSceneType) {
	// SceneFactory を使用して安全に作成・予約
	nextScene_ = SceneFactory::CreateScene(nextSceneType);
}

void SceneDirector::RequestChangeScene(std::unique_ptr<Scene> newScene) {
	nextScene_ = std::move(newScene);
}

void SceneDirector::ProcessPendingSceneChange() {
	if (!nextScene_) return;

	// 旧シーンを安全に破棄
	if (currentScene_) {
		currentScene_.reset();
	}

	RayTracingManager::GetInstance()->ClearBLASCache();

	// 新シーンの適用とセットアップ
	currentScene_ = std::move(nextScene_);

	PauseSystem::GetInstance()->SetPause(false);

	currentScene_->FngineSetUp(*p_fngine_);
	currentScene_->GetSceneDirector(this);
	currentScene_->Initialize();
}

void SceneDirector::ImGui() {
	ImGuiManager::GetInstance()->DrawSlider("DirectionalLight : pos", p_fngine_->GetLight().directionalLightData_->direction, -1.0f, 1.0f);
	ImGuiManager::GetInstance()->DrawSlider("DirectionalLight : color", p_fngine_->GetLight().directionalLightData_->color, 0.0f, 1.0f);
	//ImGuiManager::GetInstance()->DrawSlider("Dissolve : threshold", p_fngine_->dissolveForGPU_->GetMappedData()->threshold, 0.0f, 1.0f);
	//ImGuiManager::GetInstance()->DrawSlider("Dissolve : edgeMin", p_fngine_->dissolveForGPU_->GetMappedData()->edgeMin, 0.0f, 1.0f);
	//ImGuiManager::GetInstance()->DrawSlider("Dissolve : edgeMax", p_fngine_->dissolveForGPU_->GetMappedData()->edgeMax, 0.0f, 1.0f);
	PSOManager::GetInstance()->ImGui();
#ifdef USE_IMGUI
	if (ImGui::TreeNode("SceneDirector")) {
		// Enum 指定によりタイポ防止
		if (ImGui::Button("TestScene"))      RequestChangeScene(SceneType::Test);
		ImGui::SameLine();
		if (ImGui::Button("TitleScene"))     RequestChangeScene(SceneType::Title);
		ImGui::SameLine();
		if (ImGui::Button("GameScene"))      RequestChangeScene(SceneType::Game);
		ImGui::SameLine();
		if (ImGui::Button("GameOverScene"))  RequestChangeScene(SceneType::GameOver);
		ImGui::SameLine();
		if (ImGui::Button("GameClearScene")) RequestChangeScene(SceneType::Clear);

		ImGui::TreePop();
	}
	EditorManager::GetInstance()->UpdateEditors();
#endif//USE_IMGUI
}

// 
// 
// 
void SceneDirector::LoadModelData() {
	auto modelMgr = ModelManager::GetInstance();

	modelMgr->LoadAllModels("Assets/Models");

	ModelManager::GetInstance()->AddObject("Ring", MakeObjectVertices(RingData{ 32, 1.0f, 0.5f }), MakeObjectIndices(RingData{ 32, 1.0f, 0.5f }));
	ModelManager::GetInstance()->AddObject("Cylinder", MakeObjectVertices(CylinderData{ 32, 1.0f, 0.5f,1.0f }), MakeObjectIndices(CylinderData{ 32, 1.0f, 0.5f, 1.0f }));
	ModelManager::GetInstance()->AddObject("Grass", MakeObjectVertices(CylinderData{ 3, 0.05f * 0.1f, 0.5f * 0.1f,0.5f }), MakeObjectIndices(CylinderData{ 3, 0.05f * 0.1f, 0.5f * 0.1f, 0.5f }));

	AnimationManager::GetInstance()->LoadAnimationFile("Assets/Animations/Naira", "Walk.gltf");
}

void SceneDirector::LoadTexture() {
	TextureManager::GetInstance()->BeginLoad();

	std::string name;
	name = TextureManager::GetInstance()->LoadTexture("GridLine.png", "resources");
	name = TextureManager::GetInstance()->LoadTexture("ulthimaSky.png","resources");
	name = TextureManager::GetInstance()->LoadTexture("bullet.png", "resources");
	name = TextureManager::GetInstance()->LoadTexture("circle.png", "resources");
	name = TextureManager::GetInstance()->LoadTexture("Legends_Ground.png", "resources");
	name = TextureManager::GetInstance()->LoadTexture("GameOver.png", "resources");
	name = TextureManager::GetInstance()->LoadTexture("Tiles083_1K-PNG_Color.png", "Assets/Textures/Color");
	name = TextureManager::GetInstance()->LoadTexture("Tiles083_1K-PNG_NormalDX.png", "Assets/Textures/Normal");
	name = TextureManager::GetInstance()->LoadTexture("monsterBall.png", "resources");
	name = TextureManager::GetInstance()->LoadTexture("rostock_laage_airport_4k.dds", "resources");
	
	name = TextureManager::GetInstance()->LoadTexture("magic-circle.png", "resources/Texture/Effect");
	name = TextureManager::GetInstance()->LoadTexture("RevealTex_Circle.png", "resources/Texture/Effect");
	name = TextureManager::GetInstance()->LoadTexture("noise0.png", "resources/Texture/Effect");
	name = TextureManager::GetInstance()->LoadTexture("gradationLine.png", "resources/Texture/Effect");

	name = TextureManager::GetInstance()->LoadTexture("Masks_Melanin_Blood_Ratio_SSS.png", "Assets/Textures/Character");

	TextureManager::GetInstance()->EndLoad();

	//Created using <asset name> from ambientCG.com,
	//licensed under the Creative Commons CC0 1.0 Universal License.
}

void SceneDirector::LoadMusic() {
	std::string name;
	name = Music::GetInstance()->LoadSE("loop101204.wav", "resources");
}