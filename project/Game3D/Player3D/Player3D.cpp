#include "Player3D.h"
#include "CameraSystem.h"
#include "GlobalVariables.h"
#include <algorithm>
#include "AABB.h"

Player3D::~Player3D() {
	delete state_;
}
void Player3D::ApplyGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	const char* groupName = "Player";
}

void Player3D::Initialize(Fngine* fngine)
{
	obj_ = std::make_unique<ModelObject>();
	obj_->modelName_ = "walk";
	obj_->textureName_ = "ulthimaSky";
	obj_->Initialize(fngine);
	obj_->worldTransform_.set_.Scale({ 1.0f,1.0f,1.0f });

	// 始まりのState
	state_ = new PlayerStopState();
	state_->SetPlayer(this);

	attackColliderObj_ = std::make_unique<ModelObject>();
	attackColliderObj_->modelName_ = "Confuse";
	attackColliderObj_->textureName_ = "Confuse";
	attackColliderObj_->Initialize(fngine);
	attackColliderObj_->SetColor({ 0.9f,1.0f,0.1f,1.0f });

	// Colliderの設定
	// 1, Player
	playerCollider_ = std::make_unique<PlayerBodyCollider>(this);
	// 2, Attack
	attackCollider_ = std::make_unique<AttackCollider>();
	EnableHitBox(false, obj_->worldTransform_.get_.Translation());

	// UI関係
	// [ HP ]
	mainHPBar_ = std::make_unique<SpriteObject>(fngine);
	mainHPBar_->Initialize("GridLine", SPRITE_ANCHOR_TYPE::LeftMiddle);

	mainHPBar_->worldTransform_.set_.Translation({ 10.0f,20.0f,0.0f });

	subHPBar_ = std::make_unique<SpriteObject>(fngine);
	subHPBar_->Initialize("GridLine", SPRITE_ANCHOR_TYPE::LeftMiddle);
	subHPBar_->SetColor({ 1.0f,0.0f,0.0f,1.0f });
	subHPBar_->worldTransform_.set_.Translation(mainHPBar_->worldTransform_.get_.Translation());
	// [ Stamina ]
	staminaBar_ = std::make_unique<SpriteObject>(fngine);
	staminaBar_->Initialize("GridLine", SPRITE_ANCHOR_TYPE::LeftMiddle);
	staminaBar_->worldTransform_.set_.Translation({ 10.0f,20.0f + 32.0f,0.0f });
	staminaBar_->SetColor({ 0.86f,0.86f,0.0f,1.0f });

	ApplyGlobalVariables();

	animation_ = std::make_unique<Animation>();
	animation_->LoadAnimationFile("resources/Human", "walk.gltf");
	animation_->SetIsLoop(true);

	skeleton_ = std::make_unique<Skeleton>();
	skeleton_->CreateSkeleton(obj_->GetNode());

	obj_->skinCluster_.Create(fngine, *skeleton_, obj_->GetModelData());

	// 入力処理の初期化
	inputHandler_ = std::make_unique<PlayerInputHandler>();
	inputHandler_->SetPlayer(this);
}

void Player3D::Update()
{
	if (InputManager::IsLockOn()) {
		lockOn_ = !lockOn_;
	}
	CameraSystem::GetInstance()->GetActiveCamera()->SetTargetPos(
		{ obj_->worldTransform_.get_.Translation().x,obj_->worldTransform_.get_.Translation().y ,obj_->worldTransform_.get_.Translation().z });
	if (lockOn_ == false) {
		
		//CameraSystem::GetInstance()->GetActiveCamera()->SetRadius(30);
	}

	inputHandler_->HandleInput();
	ApplyPhysics();
	EvasionUpdate();
	DashBufferUpdate();
	if (state_) {
		state_->Update();
	}

	if (InputManager::GetKey().PressedKey(DIK_9)) {
		motionController_.Play("Test3",obj_->worldTransform_.get_.Translation());
	}
	if (motionController_.IsPlaying()) {
		obj_->worldTransform_.set_.Translation(motionController_.Update(1.0f / 60.0f, move_));
	}
	UpdateStaminaRecovery();

	// カメラに対してにする処理
	move_ = move_.z * CameraSystem::GetInstance()->GetActiveCamera()->zAxis_ + move_.x * CameraSystem::GetInstance()->GetActiveCamera()->xAxis_;
	move_ = Normalize(move_);
	move_.y = 1.0f;

	RotateToMoveDirection();

	Vector3 pos = obj_->worldTransform_.get_.Translation();
	float deltaTime = 1.0f / 60.0f;
	if (moveBlockers_ == 0) {
		pos.x += move_.x * speed_ * deltaTime * speedMultiplier_;
		pos.z += move_.z * speed_ * deltaTime * speedMultiplier_;
		// ※ここの性で攻撃すると空中に止まるよ
		pos.y += move_.y * verticalVelocity_;
	}
	// ここで移動範囲の制限を掛ける
	float minX = -20.0f;
	float maxX = 20.0f;
	float minZ = -20.0f;
	float maxZ = 20.0f;
	pos.x = std::clamp(pos.x, minX, maxX);
	pos.z = std::clamp(pos.z, minZ, maxZ);

	obj_->worldTransform_.set_.Translation(pos);

	// 一旦地面との当たり判定（地面を０とする）
	if (obj_->worldTransform_.get_.Translation().y <= 0.0f) {
		isOnGround_ = true;
		Vector3 pos = obj_->worldTransform_.get_.Translation();
		pos.y = 0.0f;
		obj_->worldTransform_.set_.Translation(pos);
	}

	if (isOnGround_ == true) {
		verticalVelocity_ = 0.0f;
		canDoubleJump_ = true;
	}

	HPBarUpdate();

	if (IsDead()) {
		obj_->worldTransform_.set_.Scale({ obj_->worldTransform_.get_.Scale().x - 0.01f,obj_->worldTransform_.get_.Scale().y - 0.01f ,obj_->worldTransform_.get_.Scale().z - 0.01f });
	}

	animation_->TimeFlow();
	animation_->ApplyAnimation(*skeleton_.get());
	skeleton_->Update();
	obj_->skinCluster_.Update(*skeleton_);

	ImGui();
}

void Player3D::Draw()
{
	obj_->LocalToWorld();
	obj_->SetWVPData(CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(obj_->worldTransform_.mat_));
	obj_->Draw();

	if (isAttackViewFlag_) {
		attackColliderObj_->worldTransform_.set_.Rotation({ 0.0f,attackColliderObj_->worldTransform_.get_.Rotation().y + 20.0f,0.0f });
		attackColliderObj_->worldTransform_.set_.Translation(attackCollider_->GetWorldPosition());
		attackColliderObj_->LocalToWorld();
		attackColliderObj_->SetWVPData(CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(attackColliderObj_->worldTransform_.mat_));
		attackColliderObj_->Draw();
	}

	
	subHPBar_->Draw();
	mainHPBar_->Draw();
	staminaBar_->Draw();
}

void Player3D::ImGui() {
	ImGuiManager::GetInstance()->DrawDrag("Player : Pos", obj_->worldTransform_.get_.Translation());
	ImGuiManager::GetInstance()->DrawDrag("Player : Scale", obj_->worldTransform_.get_.Scale());
	ImGuiManager::GetInstance()->DrawDrag("PlayerUV : Pos", obj_->uvTransform_.get_.Translation());
	ImGuiManager::GetInstance()->DrawDrag("stamina", stamina_);
	ImGuiManager::GetInstance()->DrawDrag("player : HP", hp_);
	ImGuiManager::GetInstance()->DrawDrag("Player : Speed", speed_);
	ImGuiManager::GetInstance()->DrawDrag("sppedMultiplier", speedMultiplier_);
	ImGuiManager::GetInstance()->DrawDrag("mat", obj_->worldTransform_.mat_);
	ImGuiManager::GetInstance()->DrawDrag("gravity", gravity_);
	ImGuiManager::GetInstance()->DrawDrag("DashBufferTime", dashBufferTimer_);
}

void Player3D::ChangeState(PlayerState* newState) {
	if (state_) {
		state_->Exit();

		delete state_;
		state_ = nullptr;
	}
	state_ = newState;
	state_->SetPlayer(this);
	state_->Initialize();
}

// ------------------------------
// AttackColliderの設定
// ------------------------------

void Player3D::EnableHitBox(bool enable, const Vector3& worldPos) {
	if (enable) {
		// 攻撃判定の位置を更新
		attackCollider_->SetWorldPosition(worldPos);

		// 攻撃判定をアクティブ化
		attackCollider_->SetMyType(COL_Player_Attack);

		// 相手のマスクも設定
		attackCollider_->SetYourType(COL_Enemy | COL_Enemy_Attack);
		ImGuiManager::GetInstance()->Text("Player Attack -> Enemy Hit!!");
	}
	else {
		// 攻撃判定を非アクティブにする
		attackCollider_->SetMyType(COL_None);
		attackCollider_->SetYourType(COL_None);
		ImGuiManager::GetInstance()->Text("Player Attack -> Enemy NO!!");
	}

}

// ------------------------------
// 物理的な処理
// ------------------------------

void Player3D::ApplyPhysics() {
	if (isOnGround_ == false) {
		if (moveBlockers_ == 0) {
			verticalVelocity_ -= gravity_ * (1.0f / 60.0f) * 0.85f;
		}
	}
}

// ------------------------------
// スタミナについて
// ------------------------------

void Player3D::UpdateStaminaRecovery() {
	if (staminaRecoveryBlockers_ > 0) {
		// Blockerが１以上なら回復しない
		return;
	}

	// 回復処理
	stamina_ += staminaRecoveryRate_ * (1.0f / 60.0f);
	if (stamina_ > maxStamina_) {
		stamina_ = maxStamina_;
	}
}

// ------------------------------
// 旋回しょり
// ------------------------------

void Player3D::RotateToMoveDirection() {
	// 死んでいたら無視
	if (IsDead()) {
		return;
	}

	// 移動処理がなければスキップ
	if (move_.x == 0.0f && move_.z == 0.0f) {
		// 入力が無かった
		return;
	}

	float targetAngle = std::atan2(move_.x, move_.z);
	
	Quaternion targetQuaternion = Quaternion::MakeRotateAxisAngleQuaternion({ 0.0f,1.0f,0.0f }, targetAngle);

	Quaternion currentQuaternion = obj_->worldTransform_.get_.Quaternion();
	currentQuaternion = Quaternion::Slerp(currentQuaternion, targetQuaternion, rotateSpeed_);
	if (std::isnan(currentQuaternion.x)) {

		return;
	}
	obj_->worldTransform_.set_.Quaternion(currentQuaternion);
}

Vector3 Player3D::GetForwardVector() {
	Vector3 ret;
	ret = obj_->worldTransform_.get_.ForwardVector();
	return (ret);
}

// ---------------------------
// 攻撃を喰らった時の処理
// ---------------------------

void Player3D::TakeDamage(float damage) {
	// ジャスト回避状態ならジャスト回避Stateに移行
	if (IsJustEvasion()) {
		ChangeState(new PlayerJustEvasionState());
		return;
	}

	// 1. 死亡状態、または無敵時間中の場合は処理をスキップ
	if (IsDead() || IsInvulnerable()) {
		return;
	}

	// 2. HPを減少させる (HPが0未満にならないように clamp する)
	hp_ -= damage;
	if (hp_ < 0.0f) {
		hp_ = 0.0f;
	}

	// 3. 死亡判定
	if (IsDead()) {
		// HPが0になったら死亡 State へ遷移
		ChangeState(new PlayerDeathState());

		// 本体Colliderを無効化（死亡後に当たり判定を残さないため）
		playerCollider_->SetMyType(COL_None);
	}
	else {
		// HPが残っている場合は被ダメージ State へ遷移
		ChangeState(new PlayerHurtState());

		// 無敵時間を設定する (HurtState側でリセットする)
		SetInvulnerable(true);
	}
}

// ---------------------------
// Command で呼び出す処理たち
// ---------------------------

void Player3D::OnJumpInput() {
	// ジャンプが禁止されているなら処理を無視
	if (CanJump() == false)return;

	// 地面にいる場合、二段ジャンプが可能ならStateを切り替える
	if (isOnGround_ || canDoubleJump_) {
		if (isOnGround_) {
			ChangeState(new PlayerJumpState());
		}
		else {
			ChangeState(new PlayerDoubleJumpState());
		}
	}
}

void Player3D::OnAttackInput() {
	// 攻撃Stateに遷移
	isAttackBuffered_ = true;
}

void Player3D::OnDashInput() {
	// ダッシュする意思があったことを「maxDashBufferTime」秒保存する
	dashBufferTimer_ = maxDashBufferTime_;
}

void Player3D::OnEvasionInput() {
	// 回避ボタンが押されたことをつたえる
	if (isWaitingEvasion_ == false) {
		evasionPressTimer_ = 0.0f;
		isWaitingEvasion_ = true;
	}
	// *** 連打した際にバグりそうで何か怖い ***
}

// ---------------------------
// 先行入力関係の処理
// ---------------------------

void Player3D::DashBufferUpdate() {
	//ダッシュ先行入力のタイマー更新
	if (dashBufferTimer_ > 0.0f) {
		dashBufferTimer_ -= (1.0f / 60.0f);
		dashBufferTimer_ = std::clamp(dashBufferTimer_, 0.0f, maxDashBufferTime_);
	}
}

void Player3D::EvasionUpdate() {
	if (isWaitingEvasion_) {
		float deltaTime = 1.0f / 60.0f;
		evasionPressTimer_ += deltaTime;// +=deltaTIme
		if (evasionPressTimer_ > deltaTime * 9.0f) {
			// 少し眺めならローリング
			ExecuteEvasion(true);
		}
		else if (InputManager::IsEvasion() == false) {
			// 時間内に離したらスライド
			ExecuteEvasion(false);
		}
	}
}

void Player3D::ExecuteEvasion(bool isLongPress) {
	isWaitingEvasion_ = false;
	evasionPressTimer_ = 0.0f;
	if (/*ロックオン &&*/isLongPress == false) {
		// ロックオンならステップ回避
		ChangeState(new PlayerStepEvasionState());
	}
	else {
		// それ以外ならローリング回避
		ChangeState(new PlayerEvasionState());
	}
}

void Player3D::ClearAttackBuffer() {
	isAttackBuffered_ = false;
}

// ------------------------
// HP関係の関数
// ------------------------

void Player3D::HPBarUpdate() {
	float hpWidth = hp_ / 16.0f * 6.2f;
	mainHPBar_->worldTransform_.set_.Scale({ hpWidth,1.0f,0.0f });
	subHPBar_->worldTransform_.set_.Scale({ Easing_Float(subHPBar_->worldTransform_.get_.Scale().x, hpWidth, 0.2f, 1.0f, EASINGTYPE::InSine),1.0f, 1.0f });
	if (hp_ > maxHP_ * 0.5f) {
		mainHPBar_->SetColor({ 0.1f,1.0f,0.2f,1.0f });
	}
	/*else if (hp_ > maxHP_ * 0.2f) {
		mainHPBar_->SetColor({ 0.65f,0.65f,0.1f,1.0f });
	}*/
	else {
		mainHPBar_->SetColor({ 0.8f,0.1f,0.1f,1.0f });
	}
	staminaBar_->worldTransform_.set_.Scale({ stamina_ / 16.0f * 6.2f, 1.0f,0.0f });
}