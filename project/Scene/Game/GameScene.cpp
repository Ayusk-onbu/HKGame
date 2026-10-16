#include "GameScene.h"
#include "ImGuiManager.h"
#include "CameraSystem.h"
#include "SceneDirector.h"
#include "Hair/IHair.h"
#include "Chronos.h"
#include "CollisionManager.h"
#include "../../UsefulTool/EditorManager/Map/SceneEditor.h"

GameScene::GameScene()
	: player_(std::make_unique<Player>()),
	  skyBox_(std::make_unique<SkyBox>())
{

}

GameScene::~GameScene() {
	
}

void GameScene::Initialize() {
	player_->Initialize(p_fngine_);

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
	purposeUI_->Initialize("GridLine");
	purposeUI_->worldTransform_.set_.Translation({ 640.0f,180.0f,0.0f });
	purposeUI_->worldTransform_.set_.Scale({ 1.0f, 1.0f,0.0f });
	purposeUI_->SetColor({ 1.0f,1.0f,1.0f,1.0f });

	playUI_ = std::make_unique<SpriteObject>(p_fngine_);
	playUI_->Initialize("UI");
	playUI_->worldTransform_.set_.Translation({ 1280.0f - 128.0f - 40.0f,64.0f + 50.0f,0.0f });
	playUI_->worldTransform_.set_.Scale({ 0.65f, 0.65f,0.0f });
	playUI_->SetColor({ 1.0f,1.0f,1.0f,1.0f });

	skyBox_->Initialize(p_fngine_, "rostock_laage_airport_4k");

	// ポーズ関係のUI
	pause_ = std::make_unique<UIContainer>();
	pause_->Initialize(p_fngine_);
	pause_->LoadbyFile("PauseBackUI");

	playGuide_ = std::make_unique<UIContainer>();
	playGuide_->Initialize(p_fngine_);
	playGuide_->LoadbyFile("PlayGuide");
	playGuide_->Play({ {"PlayGuide","PlayGuideFadeInAnim"} }, false);

	grassField_ = std::make_unique<GrassField>();
	grassField_->Initialize(p_fngine_, 5000, "RevealTex_Circle");

	column_ = std::make_unique<ColumnInstance>();

	gpuParticle_ = std::make_unique<GPUParticleSystem>();
	gpuParticle_->Initialize(p_fngine_, 1024);

	sceneMap_ = std::make_unique<SceneMap>();
	sceneMap_->Initialize();

	auto editorMgr = EditorManager::GetInstance();
	auto* sceneEditor = editorMgr->CreateEditor<SceneEditor>(sceneMap_.get());

	sceneMap_->LoadFromFile("Assets/Data/Map/TestScene.json", p_fngine_);
}

void GameScene::Update(){
	//float deltaTime = Chronos::GetInstance()->GetDeltaTime();
	//float deltaTime = Chronos::GetInstance()->GetGameDeltaTime();
	float deltaTime = 1.0f / 60.0f;
	if (playGuide_->IsEnd() == true) {
		playGuide_->Play({ {"PlayGuide","PlayGuideFadeInAnim"} }, false);
	}
	if (notGame_) {
		notGame_ = false;
		toGameTimer_ += deltaTime;
		FirstFade(toGameTimer_);
		/*if (boss_->StartCutscene(toGameTimer_)) {
			notGame_ = false;
		}*/
		ImGuiManager::GetInstance()->Text("Not Game");
	}
	else {
		// 中身をclear
		CollisionManager::GetInstance()->Begin();

		// -----------------

		player_->Update(deltaTime);

		sceneMap_->Update();

		CollisionCheck();

		ToScene();

		//grassField_->Update();

		//column_->Update();

		gpuParticle_->Update(deltaTime);

		//particle_->Update();
	}
}

void GameScene::DrawDepthPrePass() {

}

void GameScene::Draw() {

	//skySphere_->Draw();
	skyBox_->Draw();

	//boss_->Draw();
	player_->Draw();
	//particle_->Draw();
	gpuParticle_->Draw();
	//playUI_->Draw();
	// Fade
	//fadeUp_->Draw();
	//fadeDown_->Draw();
	// UI
	//purposeUI_->Draw();
	p_fngine_->hair_->Update(1.0f / 60.0f, player_->GetHeadMatrix());
}

void GameScene::CollisionCheck() {
	// 動的な物体 と 動的な物体の当たり判定をCheck!
	CollisionManager::GetInstance()->CheckAllCollisions();
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
	player_->Draw();

	pause_->Draw();
	playGuide_->Draw();
}