#include "TestScene.h"
#include "SceneDirector.h"
#include "ImGuiManager.h"
#include "CameraSystem.h"
#include "UIHEditor.h"

import MotionManager;

// ================================
// Override Functions
// ================================

TestScene::~TestScene() {
	
}

void TestScene::Initialize() {

	container_.Initialize(p_fngine_);
	

	// 初期化処理

	player_ = std::make_unique<Player3D>();
	player_->Initialize(p_fngine_);

	//particle_ = std::make_unique<Particle>(p_fngine_);
	//particle_->Initialize(1000,"water");

	ground_ = std::make_unique<ConvenienceModel>();
	ground_->Initialize(p_fngine_, "ground", "GridLine");
}

void TestScene::Update() {

	player_->Update();

	//particle_->Update();

	if (InputManager::IsJump()) {
		hasRequestedNextScene_ = true;
	}

	if (hasRequestedNextScene_) {
		p_sceneDirector_->RequestChangeScene(new TitleScene());
	}
}

void TestScene::Draw() {
	
	ground_->Draw();
	//particle_->Draw();
	player_->Draw();
}

void TestScene::PauseUpdate() {
	UIEditor::GetInstance()->SetTargetElement(&container_);
	UIEditor::GetInstance()->SetActiveAnimation(&animation_);
	UIEditor::GetInstance()->Update();

	container_.UpdateAnimation(1.0f / 60.0f);
}

void TestScene::PauseDraw() {
	container_.DrawImGui();

	MotionEditor::GetInstance()->NodeImGui();

	//Draw();
	container_.Draw();
}