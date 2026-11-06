#include "ClearScene.h"
#include "CameraSystem.h"
#include "SceneDirector.h"

void ClearScene::Initialize() {
	//sprite_.Initialize(p_fngine_->GetD3D12System(), 1280.0f, 720.0f);
	clearSprite_ = std::make_unique<SpriteObject>(p_fngine_);
	clearSprite_->Initialize("GameClear");
	clearSprite_->worldTransform_.set_.Translation({640.0f,360.0f,0.0f});
}

void ClearScene::Update() {
	if (InputManager::IsJump()) {
		isTrans_ = true;
	}

	if (isTrans_) {
		p_sceneDirector_->RequestChangeScene(SceneType::Title);
	}
}

void ClearScene::Draw() {
	clearSprite_->Draw();
}