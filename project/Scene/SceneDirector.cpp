#include "SceneDirector.h"
#include "CameraSystem.h"
#include "ModelManager.h"
#include "UIAnimation.h"
#include "Chronos.h"
#include "DrawManager.h"
#include "SDFManager.h"
#include "AnimationManager.h"
#include "SkinningManager.h"
#include "../Engine/Objects/Primitive/Box/PrimitiveBox.h"
#include "../Engine/Objects/Primitive/Ring/Ring.h"
#include "../Engine/Objects/Primitive/MagicPlane.h"
#include "../UsefulTool/EditorManager/Hair/HairEditor.h"
#include "../UsefulTool/EditorManager/Map/SceneEditor.h"
#include "../UsefulTool/EditorManager/Hermite/HermiteEditor.h"
#include "../UsefulTool/Component/ComponentFactory.h"
#include <pix3.h>

import MotionManager;

SceneDirector::~SceneDirector() {
	delete currentScene_;
}

void SceneDirector::Initialize(Scene& firstScene) {
	// Loadは何か他のクラスにまとめるべきなのではないのであろうか
	LoadTexture();
	LoadModelData();
	LoadMusic();
	UIHAnimationManager::GetInstance()->Load();
	MotionManager::GetInstance()->LoadMotions("resources/Data/Motion/Hermite/");

	SDFManager::GetInstance()->Initialize(p_fngine_);
	SDFManager::GetInstance()->BakeAll(p_fngine_->GetCommand().GetList().GetList().Get(),128);

	// 最初のシーンの初期化処理
	currentScene_ = &firstScene;
	currentScene_->FngineSetUp(*p_fngine_);
	currentScene_->Initialize();
	currentScene_->GetSceneDirector(this);

	// 使うカメラの作成
	CameraSystem::GetInstance()->MakeCamera("NormalCamera", CameraType::Normal);
	CameraSystem::GetInstance()->MakeCamera("DebugCamera", CameraType::Debug);
	CameraSystem::GetInstance()->MakeCamera("GameCamera", CameraType::Game);
	CameraSystem::GetInstance()->SetActiveCamera("DebugCamera");

	ComponentFactory::GetInstance()->Initialize();

	DrawManager::GetInstance()->Initialize(p_fngine_);

	auto editorMgr = EditorManager::GetInstance();
	auto* hairEditor = editorMgr->CreateEditor<HairGuideEditor>(p_fngine_->hair_.get());
	auto* hermiteEditor = editorMgr->CreateEditor<HermiteEditor>();
	editorMgr->SetActiveEditor(hermiteEditor);
}

void SceneDirector::Update() {
	// 更新処理

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

void SceneDirector::ImGui() {
	ImGuiManager::GetInstance()->DrawSlider("DirectionalLight : pos", p_fngine_->GetLight().directionalLightData_->direction, -1.0f, 1.0f);
	ImGuiManager::GetInstance()->DrawSlider("DirectionalLight : color", p_fngine_->GetLight().directionalLightData_->color, 0.0f, 1.0f);
	//ImGuiManager::GetInstance()->DrawSlider("Dissolve : threshold", p_fngine_->dissolveForGPU_->GetMappedData()->threshold, 0.0f, 1.0f);
	//ImGuiManager::GetInstance()->DrawSlider("Dissolve : edgeMin", p_fngine_->dissolveForGPU_->GetMappedData()->edgeMin, 0.0f, 1.0f);
	//ImGuiManager::GetInstance()->DrawSlider("Dissolve : edgeMax", p_fngine_->dissolveForGPU_->GetMappedData()->edgeMax, 0.0f, 1.0f);
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
	EditorManager::GetInstance()->UpdateEditors();
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
	TextureManager::GetInstance()->EndLoad();

	//Created using <asset name> from ambientCG.com,
	//licensed under the Creative Commons CC0 1.0 Universal License.
}

void SceneDirector::LoadMusic() {
	std::string name;
	name = Music::GetInstance()->LoadSE("loop101204.wav", "resources");
}