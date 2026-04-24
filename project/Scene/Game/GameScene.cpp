#include "GameScene.h"
#include "ImGuiManager.h"
#include "CameraSystem.h"
#include "SceneDirector.h"


GameScene::GameScene()
	: player_(std::make_unique<Player3D>()),
	  boss_(std::make_unique<BossEnemy>()),
	  collisionManager_(std::make_unique<CollisionManager>()),
	  gameMap_(std::make_unique<GameMap>())
{

}

GameScene::~GameScene() {
	
}

void GameScene::Initialize() {
	player_->Initialize(p_fngine_);
	boss_->Initialize(p_fngine_,player_.get());
	gameMap_->Initialize(p_fngine_);

	toGameTimer_ = 0.0f;
	// Fade関係のUI
	fadeUp_ = std::make_unique<SpriteObject>(p_fngine_);
	fadeUp_->Initialize("GridLine");
	fadeUp_->worldTransform_.set_.Translation({ 640.0f,180.0f,0.0f });
	fadeUp_->worldTransform_.set_.Scale({ 1280.0f / 16.0f, 360.0f / 16.0f,0.0f });
	fadeUp_->SetColor({ 0.0f,0.0f,0.0f,1.0f });

	fadeDown_ = std::make_unique<SpriteObject>(p_fngine_);
	fadeDown_->Initialize("GridLine");
	fadeDown_->worldTransform_.set_.Translation({ 640.0f,180.0f + 360.0f,0.0f });
	fadeDown_->worldTransform_.set_.Scale({ 1280.0f / 16.0f, 360.0f / 16.0f,0.0f });
	fadeDown_->SetColor({ 0.0f,0.0f,0.0f,1.0f });

	purposeUI_ = std::make_unique<SpriteObject>(p_fngine_);
	purposeUI_->Initialize("Purpose");
	purposeUI_->worldTransform_.set_.Translation({ 640.0f,180.0f,0.0f });
	purposeUI_->worldTransform_.set_.Scale({ 1.0f, 1.0f,0.0f });
	purposeUI_->SetColor({ 1.0f,1.0f,1.0f,1.0f });

	playUI_ = std::make_unique<SpriteObject>(p_fngine_);
	playUI_->Initialize("UI");
	playUI_->worldTransform_.set_.Translation({ 1280.0f - 128.0f - 40.0f,64.0f + 50.0f,0.0f });
	playUI_->worldTransform_.set_.Scale({ 0.65f, 0.65f,0.0f });
	playUI_->SetColor({ 1.0f,1.0f,1.0f,1.0f });

	skySphere_ = std::make_unique<ConvenienceModel>();
	skySphere_->Initialize(p_fngine_, "ulthimaSky", "ulthimaSky");


	// ポーズ関係のUI
	pause_ = std::make_unique<UIContainer>();
	pause_->Initialize(p_fngine_);
	pause_->LoadbyFile("PauseBackUI");

	playGuide_ = std::make_unique<UIContainer>();
	playGuide_->Initialize(p_fngine_);
	playGuide_->LoadbyFile("PlayGuide");
	playGuide_->Play({ {"PlayGuide","PlayGuideFadeInAnim"} }, false);
}

void GameScene::Update(){
	float deltaTime = 1.0f / 60.0f;
	if (playGuide_->IsEnd() == true) {
		playGuide_->Play({ {"PlayGuide","PlayGuideFadeInAnim"} }, false);
	}
	if (notGame_) {
		notGame_ = false;
		toGameTimer_ += deltaTime;
		FirstFade(toGameTimer_);
		if (boss_->StartCutscene(toGameTimer_)) {
			notGame_ = false;
		}
		ImGuiManager::GetInstance()->Text("Not Game");
	}
	else {
		player_->Update();

		//boss_->Update();

		gameMap_->Update();

		CollisionCheck();

		ToScene();
	}
}

void GameScene::Draw() {
	skySphere_->Draw();
	gameMap_->Draw();

	boss_->Draw();
	player_->Draw();

	playUI_->Draw();
	// Fade
	fadeUp_->Draw();
	fadeDown_->Draw();
	// UI
	purposeUI_->Draw();

}

void GameScene::CollisionCheck() {
	// 中身をclear
	collisionManager_->Begin();

	// マップの情報を登録
	collisionManager_->SetMap(gameMap_->GetBVH());

	// ここからColliderを設定
	collisionManager_->SetColliders(player_->collider_.get());

	// Map と 動的な物体（Player等）当たり判定をCheck！
	collisionManager_->CheckMapCollisions();

	// 動的な物体 と 動的な物体の当たり判定をCheck!
	collisionManager_->CheckAllCollisions();
}

void GameScene::ToScene() {
	if (toSceneTimer_ < 3.0f) {
		// timeを0にして使う
		Vector3 upPos = fadeUp_->worldTransform_.get_.Translation();
		upPos.y = Easing_Float(-180.0f, 180.0f, toSceneTimer_, 3.0f, EASINGTYPE::InBounce);
		fadeUp_->worldTransform_.set_.Translation(upPos);

		Vector3 downPos = fadeDown_->worldTransform_.get_.Translation();
		downPos.y = Easing_Float((180.0f) + 720.0f, 180.0f + 360.0f, toSceneTimer_, 3.0f, EASINGTYPE::InBounce);
		fadeDown_->worldTransform_.set_.Translation(downPos);
	}
	if (boss_->IsDead()) {
		ToClearScene();
	}
	else if (player_->IsDead()) {
		ToGameOverScene();
	}
}

void GameScene::ToClearScene() {
	toSceneTimer_ += 1.0f / 60.0f;

	if (toSceneTimer_ > 3.0f) {
		p_sceneDirector_->RequestChangeScene(new ClearScene());
		return;
	}
}

void GameScene::ToGameOverScene() {
	toSceneTimer_ += 1.0f / 60.0f;

	if (toSceneTimer_ > 3.0f) {
		p_sceneDirector_->RequestChangeScene(new GameOverScene());
		return;
	}
}

void GameScene::FirstFade(float time) {
	if (time < 0.2f) {
		// 開ける
		Vector3 upPos = fadeUp_->worldTransform_.get_.Translation();
		upPos.y = Easing_Float(180.0f, 0.0f, time, 0.2f, EASINGTYPE::None);
		fadeUp_->worldTransform_.set_.Translation(upPos);

		Vector3 downPos = fadeDown_->worldTransform_.get_.Translation();
		downPos.y = Easing_Float(180.0f + 360.0f, 720.0f, time, 0.2f, EASINGTYPE::None);
		fadeDown_->worldTransform_.set_.Translation(downPos);
	}
	else if (time < 3.0f && time > 2.0f) {
		// timeを0にして使う
		float fadeOutTime = time - 2.0f;
		Vector3 upPos = fadeUp_->worldTransform_.get_.Translation();
		upPos.y = Easing_Float(0.0f, -180.0f, fadeOutTime, 1.0f, EASINGTYPE::InBounce);
		fadeUp_->worldTransform_.set_.Translation(upPos);

		Vector3 downPos = fadeDown_->worldTransform_.get_.Translation();
		downPos.y = Easing_Float(720.0f, (180.0f) + 720.0f, fadeOutTime, 1.0f, EASINGTYPE::InBounce);
		fadeDown_->worldTransform_.set_.Translation(downPos);
	}
	float alpha = 1.0f;
	alpha = Easing_Float(1.0f, 0.0f, toGameTimer_, 3.0f, EASINGTYPE::None);
	purposeUI_->SetColor({ 1.0f,1.0f,1.0f,alpha });
}

void GameScene::PauseUpdate() {
	playGuide_->UpdateAnimation(1.0f / 60.0f);

	if (InputManager::GetGamePad(0).IsPressed(XINPUT_GAMEPAD_Y)) {
		p_sceneDirector_->RequestChangeScene(new TitleScene());
	}
}

void GameScene::PauseDraw() {
	gameMap_->Draw();
	boss_->Draw();
	player_->Draw();

	pause_->Draw();
	playGuide_->Draw();
}