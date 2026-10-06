#include "Player.h"
#include "DrawManager.h"
#include "Component/Controller/Controller.h"
#include "Component/State/Movement/Movement.h"
#include "Component/State/Movement/CommonMovementActions.h"
#include "Component/Skinning/SkinningComponent.h"
#include "Component/Status/Status.h"
#include "Component/Physics/ColliderComponent.h"
#include "Component/Physics/RigidBody.h"
#include "Component/Render/RenderComponent.h"

Player::Player() {
	//controller_ = std::make_unique<PlayerController>();
}

void Player::Initialize(Fngine* fngine) {
	DynamicObject::Initialize(fngine, "Naira_ExportTest", "ulthimaSky");
	//transform_.set_.Rotation({ 0.0f,180.0f,0.0f });

	auto* render = AddComponent<RenderComponent>();
	render->SetProvider<CharacterRenderProvider>();

	// =========================================================
	// 脳みそ (ControllerComponent) ➔ プレイヤー操作を代入
	// =========================================================
	auto* ctrl = AddComponent<ControllerComponent>();
	ctrl->SetController(std::make_unique<PlayerController>(), "PlayerController");

	// =========================================================
	// 足腰 (MovementComponent) ➔ プレイヤー用のアクションを構築
	// =========================================================
	auto* moveComp = AddComponent<MovementComponent>();
	moveComp->AddAction<IdleMovementAction>();
	moveComp->AddAction<WalkMovementAction>();
	moveComp->AddAction<AirMovementAction>();
	moveComp->AddAction<JumpMovementAction>(9.0f); // ジャンプ力: 9.0f

	// =========================================================
	// 物理 ＆ ステータスコンポーネント
	// =========================================================
	auto* rb = AddComponent<RigidBody>();
	rb->SetBodyType(BodyType::Kinematic); // キャラクター物理
	rb->SetUseGravity(false);

	auto* status = AddComponent<StatusComponent>();
	status->SetBaseStats(1000.0f, 100.0f, 50.0f, 6.0f); // HP, ATK, DEF, Speed

	// =========================================================
	// スキニング (SkinningComponent) ➔ 自動アニメーション
	// =========================================================
	auto* skinning = AddComponent<SkinningComponent>();
	if (skinning->Setup(fngine, "Naira_ExportTest")) {
		// デフォルト待機アニメーションを再生
		skinning->PlayAnimation("Armature|mixamo.com|Layer0 Retarget", true);
	}

	// =========================================================
	// 当たり判定 (ColliderComponent)
	// =========================================================
	auto* col = AddComponent<ColliderComponent>();
	col->SetLayer("Player");
	col->SetTargetLayers({ "Enemy", "StaticMap" });

	EventManager::GetInstance()->RegisterAction(
		EVENTCATEGORY::EFFECT, 0, this, &Player::MakeHitEffect
	);
	EventManager::GetInstance()->BindEventToTag(GAMEEVENTID::OnPlayerAttack, EVENTCATEGORY::EFFECT, 0);

	EventManager::GetInstance()->RegisterAction(
		EVENTCATEGORY::UI, 0, this, &Player::SetHeadPosToCameraTarget
	);
	EventManager::GetInstance()->BindEventToTag(GAMEEVENTID::HairEditor, EVENTCATEGORY::UI, 0);
}

void Player::Update(float deltaTime) {
	DynamicObject::Update(deltaTime);


	MakeHitEffect();

	for (auto& effect : hitEffect_) {
		// エフェクトの更新
		effect.currentTime += deltaTime;
		effect.color.w = 1.0f - (effect.currentTime / effect.lifeTime); // 徐々に透明にする

		DrawManager::GetInstance()->GetBox()->AddInstance({
			effect.transform,
			effect.color
		});
	}

	hitEffect_.erase(std::remove_if(hitEffect_.begin(), hitEffect_.end(),
		[](const HitEffectInfo& effect) { return effect.currentTime >= effect.lifeTime; }),
		hitEffect_.end());

	// この処理はここに書きたくない
	/*CameraSystem::GetInstance()->GetActiveCamera()->SetTargetPos(
		{ obj_->worldTransform_.get_.Translation().x,obj_->worldTransform_.get_.Translation().y + 1.0f ,obj_->worldTransform_.get_.Translation().z }
	);*/
	//SetHeadPosToCameraTarget();
}

void Player::Draw() {
	//obj_->LocalToWorld();
	DynamicObject::Draw();
}

void Player::MakeHitEffect() {
	hitEffectCoolTimer_ -= 1.0f / 60.0f;
	if (hitEffectCoolTimer_ > 0.0f)return;
	auto rand = RandomUtils::GetInstance();

	Vector3 headPos = GetHeadPos();

	for (int i = 0; i < 8; ++i) {
		HitEffectInfo info;
		info.transform.Initialize();
		//info.transform.set_.Scale({});
		info.transform.set_.Rotation({ ((float)rand->GetHighRandom().GetInt(0,360)),180.0f, ((float)rand->GetHighRandom().GetInt(0,360)) });
		//info.transform.set_.Translation({ obj_->worldTransform_.GetWorldPos().x,obj_->worldTransform_.GetWorldPos().y + 0.2f,obj_->worldTransform_.GetWorldPos().z });
		info.transform.LocalToWorld();
		info.lifeTime = 1.0f;
		info.currentTime = 0.0f;
		info.color = Vector4(1.0f, 0.0f, 0.0f, 1.0f);
		hitEffect_.push_back(info);
	}
	hitEffectCoolTimer_ = 0.2f;
}

Matrix4x4 Player::GetHeadMatrix()const {
	if (auto* skinning = GetComponent<SkinningComponent>()) {
		// "Head" ボーンのワールド行列を取得（skeletonSpaceMatrix * キャラクターのワールド行列）
		if (auto headWorldMatOpt = skinning->GetJointWorldMatrix("Head")) {
			return *headWorldMatOpt;
		}
	}

	// スキニングコンポーネントがない、またはHeadボーンが見つからない場合のフォールバック（自身のワールド行列）
	return transform_.mat_;
}

Vector3 Player::GetHeadPos()const {
	if (auto* skinning = GetComponent<SkinningComponent>()) {
		// "Head" ボーンのワールド座標を取得
		if (auto posOpt = skinning->GetJointWorldPosition("Head")) {
			return *posOpt + Vector3(0.0f, 0.075f, 0.0f);
		}
	}
	// フォールバック（トランスフォームから補正）
	Vector3 headPos = transform_.transform_.translation_ + Vector3(0.0f, 1.5f, 0.0f);
	return headPos;
}

void Player::SetHeadPosToCameraTarget() {
	auto headPos = GetHeadPos();
	CameraSystem::GetInstance()->GetActiveCamera()->SetTargetPos(headPos);
}

void Player::TakeDamage() {
	//status_.TakeDamage(5.0f, Element::Fire);

	//EventManager::GetInstance()->FireEvent(GAMEEVENTID::PlayerTakeDamage);
}