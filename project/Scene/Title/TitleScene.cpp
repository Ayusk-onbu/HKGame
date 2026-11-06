#include "TitleScene.h"
#include "SceneDirector.h"

TitleScene::~TitleScene() {}

void TitleScene::Initialize() {
	// 初期化処理

	// [ タイトル画面 ]
	titles_ = std::make_unique<UIContainer>();
	titles_->Initialize(p_fngine_);
	titles_->LoadbyFile("TestContainer");
	std::unordered_map<std::string, std::string>animMap = {
		{"TestContainer","TitleStay"},
		{"container : firstContainer","RandS"},
		{"element : firstElement","RandS"}
	};
	titles_->Play(animMap);
	// [ フェード ]
	fade_ = std::make_unique<SpriteObject>(p_fngine_);
	fade_->Initialize("GridLine");
	fade_->worldTransform_.set_.Translation({ 640.0f,360.0f,0.0f });
	fade_->worldTransform_.set_.Scale({ 1280.0f / 16.0f,1280.0f / 16.0f ,0.0f });
	fade_->SetColor({ 0.0f,0.0f,0.0f,0.0f });

	toGameTimer_ = 0.0f;
}

void TitleScene::Update() {
	// 初期化処理
	titles_->UpdateAnimation(1.0f / 60.0f);
	if (titles_->AllEnded()) {
		std::unordered_map<std::string, std::string>animMap = {
		{"TestContainer","Stay"},
		{"container : firstContainer","TitleStay"},
		{"element : firstElement","RandS"}
		};
		titles_->Play(animMap);
	}

	// [ シーン遷移処理 ] 
	if (InputManager::IsJump()) {
		hasRequestedNextScene_ = true;
	}
	if (hasRequestedNextScene_) {
		toGameTimer_ += 1.0f / 60.0f;
		float alpha = 0.0f;
		float endTime = 2.0f;
		alpha = Easing_Float(0.0f, 1.0f, toGameTimer_, endTime, EASINGTYPE::InSine);
		fade_->SetColor({ 0.0f,0.0f,0.0f,alpha });
		if (toGameTimer_ > endTime) {
			p_sceneDirector_->RequestChangeScene(SceneType::Game);
		}
	}
}

void TitleScene::Draw() {
	// 描画処理 
	titles_->Draw();
	fade_->Draw();
}