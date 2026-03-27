#include "CameraSystem.h"
#include "ImGuiManager.h"
#include "Player.h"
#include "Ground.h"
#include <string>

using namespace PlayerStates;
void Player::Initialize() {
	obj_ = std::make_unique<ModelObject>();
	obj_->modelName_ = "player";
	obj_->textureName_ = "ulthimaSky";
	obj_->Initialize(p_fngine);

	InitializeStates();
	InitializeComponents();

	inputHandler_.SetPlayer(this);

	rightHandJoint_.SetType(AttachmentType::PlayerHand);
	rightHandJoint_.SetAcceptType(AttachmentType::UmbrellaHandle);
	rightHandJoint_.SetInfo({ 0.0f,-0.0f,0.0f }, { 0.0f,0.0f,0.0f });

	backJoint_.SetType(AttachmentType::PlayerBack);
	backJoint_.SetAcceptType(AttachmentType::UmbrellaHandle);
	backJoint_.SetInfo({ 0.0f,-0.0f,0.0f }, { 0.0f,0.0f,0.0f });
	backJoint_.SetRot({0.0f,0.0f,Deg2Rad(135)});

	umbrella_ = std::make_unique<Umbrella::Main>();
	umbrella_->Initialize(p_fngine);

	umbrella_->handle_->GetBaseJoint()->AttachTo(GetRightHandJoint());

	motionController_ = std::make_unique<MotionController>();

	// =====================
	// 【 当たり判定の設定 】
	// =====================
	
	// 1. コライダーの生成
	collider_ = std::make_unique<ConvexCollider>();
	collider_->SetUserData(this);

	// 2. 属性の設定（自分はPlayer、当たる相手はEnemyやEnemyの攻撃）
	collider_->SetMyType(COL_Player);
	collider_->SetYourType(COL_Enemy | COL_Enemy_Attack | COL_Ground);

	// 3. ローカル頂点データの設定（例：プレイヤーを囲む四角形やひし形など）
	std::vector<Vector3> localVertices = {
		{-1.0f, -1.0f, 0.0f}, // 左下
		{ 1.0f, -1.0f, 0.0f}, // 右下
		{-1.0f,  1.0f, 0.0f}, // 左上
		{ 1.0f,  1.0f, 0.0f}  // 右上
	};
	collider_->SetVertices(localVertices);

	// 4. 当たった時の処理（コールバック関数の登録）
	// ラムダ式を使って、このPlayerのメンバ関数や変数にアクセスできるようにする
	collider_->onCollisionCallback = [this](Collider* other, const Vector3& pushOut) {
		if (other->GetMyType() == COL_Ground) {

			ImGuiManager::GetInstance()->Text("Player to Ground Collision!!");

			// =========================
			// 【 めり込み解消処理 】
			// =========================
			Vector3 actualPush = { -pushOut.x, -pushOut.y, -pushOut.z };
			Vector3 pos = obj_->worldTransform_.get_.Translation();
			pos.x += actualPush.x;
			pos.y += actualPush.y;
			pos.z += actualPush.z;
			obj_->worldTransform_.set_.Translation(pos);

			Vector3 normal = actualPush;
			float length = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
			if (length > 0.0f) {
				normal.x /= length;
				normal.y /= length;
				normal.z /= length;
			}

			// 足元に地面があるかのチェック
			if (normal.y > 0.8f) {
				// 「落下中」または「立ち止まっている」時だけ着地判定
				// ジャンプ上昇中（> 0.0f）は坂に触れても着地しないようにする
				if (this->externalVelocity_.y <= 0.0f) {
					ImGuiManager::GetInstance()->Text("Player to Ground Collision!! -> OKOKOKO");
					this->onGround_ = true;

					// 地面の上なのでリセット
					if (this->externalVelocity_.y < 0.0f) {
						this->externalVelocity_.y = 0.0f;
					}
				}
			}

			Ground* ground = static_cast<Ground*>(other->GetUserData());
			if (ground != nullptr) {

			}
		}
		else if (other->GetMyType() == COL_Enemy_Attack) {

			// 1. 相手のコライダーから「持ち主（Enemy）」のポインタをもらう
			// ※ void* で返ってくるので、Enemy型にキャスト（変換）する
			//Enemy* enemy = static_cast<Enemy*>(other->GetUserData());

			// 2. 万が一キャストに失敗していないかチェック
			//if (enemy != nullptr) {
			//	// 3. 敵本体から攻撃力を取得して、ダメージを受ける！
			//	int damage = enemy->GetAttackPower();
			//	this->TakeDamage(damage); // プレイヤーのHPを減らす処理など
			//}
		}
	};
}

void Player::Update(float deltaTime) {
	// 移動量の初期化
	moveAmount_ = { 0.0f,0.0f,0.0f };

	// 入力関係の処理
	inputHandler_.HandleInput();

	// コンポーネントの更新
	if (mana_) {
		mana_->Update(deltaTime);
	}
	// ステートの更新
	if (currentMovementState_) {
		currentMovementState_->Update(deltaTime);
	}
	if (currentActionState_) {
		currentActionState_->Update(deltaTime);
	}

	// =========================
	// 【 コヨーテタイムの処理 】
	// =========================
	if (this->onGround_) {
		jumpCoyoteTimer_ = 0.0f;
	}
	else {
		jumpCoyoteTimer_ += deltaTime;
	}

	// ここから移動関係の処理
	moveAmount_ = (myVelocity_ + externalVelocity_) * deltaTime;

	obj_->worldTransform_.set_.Translation(obj_->worldTransform_.get_.Translation() + moveAmount_);

	// rightHandJoint_.SetRot( 手の回転 );
	rightHandJoint_.Update(); // 右手Joint自身の行列を計算

	Vector3 backPos = obj_->worldTransform_.get_.Translation();
	backPos.y += 1.5f;
	backPos.z += 1.0f;

	backJoint_.SetPos(backPos);
	backJoint_.Update();
	
	// 傘
	umbrella_->Update(deltaTime);

	// Colliderに設定
	collider_->SetWorldPosition(GetPosition());

	collider_->SetWorldMatrix(obj_->worldTransform_.mat_);

	collider_->UpdateAABB();

	Vector3 test = rightHandJoint_.GetPos();
	ImGui::DragFloat3("RHandJoint", &test.x);

	Vector3 colliderPos = collider_->GetWorldPosition();
	ImGui::DragFloat3("colliderPos", &colliderPos.x);

	ImGuiManager::GetInstance()->DrawDrag("Player : External Speed", this->externalVelocity_);
	ImGuiManager::GetInstance()->DrawDrag("Player : My Speed", this->myVelocity_);

	if (ImGui::TreeNodeEx("Mana")) {
		ImGui::Text("Use : Push LSHIFT");
		ImGui::Text("Mana is Use ? : ");
		ImGui::SameLine();
		ImGui::Text(inputData_.useMana ? "Using" : "Not Use");

		float mana = mana_->GetCurrentMana();
		ImGui::DragFloat("Amount", &mana);
		ImGui::TreePop();
	}

	// 地面についているフラグを解除
	// ※ バグの原因になりそうな箇所
	this->onGround_ = false;
}

void Player::Draw() {
	obj_->LocalToWorld();
	obj_->SetWVPData(CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(obj_->worldTransform_.mat_));
	obj_->Draw();

	umbrella_->Draw();
}

void Player::ChangeMovementState(PlayerStates::Base* newState) {
	if (currentMovementState_) {
		currentMovementState_->Exit();
	}

	currentMovementState_ = newState;

	if (currentMovementState_) {
		currentMovementState_->Enter();
	}
}

void Player::ChangeActionState(PlayerStates::Base* newState) {
	if (currentActionState_) {
		currentActionState_->Exit();
	}

	currentActionState_ = newState;

	if (currentActionState_) {
		currentActionState_->Enter();
	}
}

void Player::InitializeStates() {

	//// MovementStateの初期化 ////

	// 親のStateの生成
	groundedState_ = std::make_unique<Movement::Grounded>();groundedState_->SetInfo(this);
	airborneState_ = std::make_unique<Movement::Airborne>();airborneState_->SetInfo(this);

	// 子のStateの生成
	idleState_ = std::make_unique<Movement::Idle>();idleState_->SetInfo(this, groundedState_.get());
	walkingState_ = std::make_unique<Movement::Walking>();walkingState_->SetInfo(this, groundedState_.get());
	runningState_ = std::make_unique<Movement::Running>();runningState_->SetInfo(this, groundedState_.get());
	restrictedState_ = std::make_unique<Movement::Restricted>();restrictedState_->SetInfo(this, groundedState_.get());

	// 最初の設定
	currentMovementState_ = idleState_.get();

	//// ActionStateの初期化 ////

	sheatheWeaponState_ = std::make_unique<Action::SheatheWeapon>();sheatheWeaponState_->SetInfo(this);
	drawWeaponState_ = std::make_unique<Action::DrawWeapon>();drawWeaponState_->SetInfo(this);
	normalState_ = std::make_unique<Action::Normal>();normalState_->SetInfo(this);
	attackState_ = std::make_unique<Action::Attack>();attackState_->SetInfo(this);
	guardState_ = std::make_unique<Action::Guard>();guardState_->SetInfo(this);

	umbrellaOpenState_ = std::make_unique<Action::UmbrellaOpen>();umbrellaOpenState_->SetInfo(this);
	umbrellaCloseState_ = std::make_unique<Action::UmbrellaClose>();umbrellaCloseState_->SetInfo(this);
	umbrellaReverseState_ = std::make_unique<Action::UmbrellaReverse>();umbrellaReverseState_->SetInfo(this);

	// 最初の設定
	currentActionState_ = normalState_.get();
}

void Player::InitializeComponents() {
	//// ManaComponentの初期化 ////
	mana_ = std::make_unique<ManaComponent>(100.0f);

}
///////////////////
///
///   移動関係
///
///////////////////
void Player::AddForce(const Vector3& force) {
	externalVelocity_ += force;
}

void Player::Jump() {
	if (inputData_.isJump) {
		if (this->onGround_ || this->jumpCoyoteTimer_ < JUMP_COYOTE_MAX_TIME) {
			// Y軸に上向きの初速（ジャンプ力）を与える！
			float jumpPower = 7.0f; // 調整
			externalVelocity_.y = jumpPower;// 初速
			// フラグの処理
			this->onGround_ = false;
			this->jumpCoyoteTimer_ = this->JUMP_COYOTE_MAX_TIME;

			// ステートを「空中」に切り替える！
			ChangeMovementState(airborneState_.get());
		}
	}
}

void Player::UmbrellaAttachBack() {
	umbrella_->handle_->GetBaseJoint()->AttachTo(GetBackJoint());
}

void Player::UmbrellaAttachRHand() {
	umbrella_->handle_->GetBaseJoint()->AttachTo(GetRightHandJoint());
}