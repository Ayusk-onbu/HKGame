#include "ParticleEmitter.h"
#include "Particle.h"
#include "CameraSystem.h"

void ParticleEmitter::Initialize(Fngine* fngine) {
	fngine_ = fngine;
#ifdef _DEBUG
	obj_ = std::make_unique<ModelObject>();
	obj_->modelName_ = "bullet";
	obj_->textureName_ = "bullet";
	obj_->Initialize(fngine);
#endif// _DEBUG
}

void ParticleEmitter::Update(Particle* particleSystem) {
	// *** 更新処理 ***
	// [ 発生 ]
	for (uint32_t i = 0;i < emitNum_;++i) {
		Emit(particleSystem);
	}
}

void ParticleEmitter::Emit(Particle* particleSystem) {
	// *** 発生処理 ***

	// particleSystem (Particleクラス)のinfo_リストにアクセスして生成
	if (particleSystem->GetInfoList().size() < particleSystem->GetNumMaxInstance()) {
		std::unique_ptr<ParticleData> info = std::make_unique<ParticleData>();

		// [ 位置とスケール ]
		info->worldTransform.Initialize();

		// [ エミッターの位置を基準に、spawnRadius_分ランダムにずらした位置を初期位置とする ]
		Vector3 spawnOffset = {
			RandomUtils::GetInstance()->GetHighRandom().GetFloat(-spawnRadius_, spawnRadius_),
			RandomUtils::GetInstance()->GetHighRandom().GetFloat(-spawnRadius_, spawnRadius_),
			RandomUtils::GetInstance()->GetHighRandom().GetFloat(-spawnRadius_, spawnRadius_)
		};

		info->worldTransform.set_.Translation(worldTransform_.transform_.translation_ + spawnOffset);
		info->worldTransform.LookAtToTarget(CameraSystem::GetInstance()->GetActiveCamera()->GetTranslation());

		info->startScale = RandomUtils::GetInstance()->GetHighRandom().GetFloat(1.0f, 1.5f);
		info->endScale = RandomUtils::GetInstance()->GetHighRandom().GetFloat(0.15f, 0.25f);

		// [ 速度 ]
		info->velocity = {
			RandomUtils::GetInstance()->GetHighRandom().GetFloat(minVelocity_.x, maxVelocity_.x),
			RandomUtils::GetInstance()->GetHighRandom().GetFloat(minVelocity_.y, maxVelocity_.y),
			RandomUtils::GetInstance()->GetHighRandom().GetFloat(minVelocity_.z, maxVelocity_.z)
		};

		// [ 色と寿命 ]
		info->startColor = startColor_;
		info->endColor = endColor_;

		info->lifeTime = RandomUtils::GetInstance()->GetHighRandom().GetFloat(minLifeTime_, maxLifeTime_);
		info->currentTime = 0.0f;

		// WorldTransformの初期化を完了
		info->worldTransform.LocalToWorld();

		// ParticleSystemのリストに追加
		particleSystem->AddParticle(std::move(info));
	}
}

void ParticleEmitter::DrawDebug() {
#ifdef _DEBUG
	obj_->worldTransform_.set_.Translation(worldTransform_.get_.Translation());
	obj_->worldTransform_.set_.Scale({ spawnRadius_,spawnRadius_,spawnRadius_ });
	obj_->LocalToWorld();
	obj_->SetWVPData(CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(obj_->worldTransform_.mat_));
	obj_->Draw(ObjectDrawType::WIREFRAME);
#endif// _DEBUG
}

void ParticleEmitter::DrawImGui() {
	// *** 調整 ***
#ifdef USE_IMGUI
	// [ 位置情報の更新 ]
	Vector3 pos = worldTransform_.get_.Translation();
	int num = emitNum_;
	ImGui::DragFloat3("pos", &pos.x);
	ImGui::DragFloat("spawnRadius", &spawnRadius_, 0.01f, 0.0f);
	spawnRadius_ = std::clamp(spawnRadius_, 0.0f, 1000.0f);
	ImGui::DragFloat("minLife", &minLifeTime_, 1.0f, 0.0f);
	ImGui::DragFloat("maxLife", &maxLifeTime_);
	maxLifeTime_ = minLifeTime_ > maxLifeTime_ ? minLifeTime_ : maxLifeTime_;
	ImGui::DragFloat3("minSpeed", &minVelocity_.x, 0.01f);
	ImGui::DragFloat3("maxSpeed", &maxVelocity_.x, 0.01f);
	ImGui::SliderInt("emitNum", &num, 0, 100);
	ImGui::ColorEdit4("Start color", &startColor_.x);
	ImGui::ColorEdit4("End color", &endColor_.x);
	emitNum_ = num;
	worldTransform_.set_.Translation(pos);
#endif
}

json ParticleEmitter::SaveData() {
	json data;
	{
		data["name"] = name_;
		data["translation"] = {
			worldTransform_.transform_.translation_.x,
			worldTransform_.transform_.translation_.y,
			worldTransform_.transform_.translation_.z
		};
		data["emitNum"] = emitNum_;
		data["spawnRadius"] = spawnRadius_;
		data["minLife"] = minLifeTime_;
		data["maxLife"] = maxLifeTime_;
		data["minVelocity"] = {
			minVelocity_.x,
			minVelocity_.y,
			minVelocity_.z
		};
		data["maxVelocity"] = {
			maxVelocity_.x,
			maxVelocity_.y,
			maxVelocity_.z
		};
		data["startColor"] = {
			startColor_.x,
			startColor_.y,
			startColor_.z,
			startColor_.w
		};
		data["endColor"] = {
			endColor_.x,
			endColor_.y,
			endColor_.z,
			endColor_.w
		};
	}
	return data;
}

void ParticleEmitter::LoadData(const json& data) {
	name_ = data["name"];
	auto& pos = data["translation"];
	worldTransform_.set_.Translation({ pos[0], pos[1], pos[2] });
	emitNum_ = data["emitNum"];
	spawnRadius_ = data["spawnRadius"];
	minLifeTime_ = data["minLife"];
	maxLifeTime_ = data["maxLife"];
	auto& minVelocity = data["minVelocity"];
	minVelocity_.x = minVelocity[0];
	minVelocity_.y = minVelocity[1];
	minVelocity_.z = minVelocity[2];
	auto& maxVelocity = data["maxVelocity"];
	maxVelocity_.x = maxVelocity[0];
	maxVelocity_.y = maxVelocity[1];
	maxVelocity_.z = maxVelocity[2];
	auto& startColor = data["startColor"];
	startColor_.x = startColor[0];
	startColor_.y = startColor[1];
	startColor_.z = startColor[2];
	startColor_.w = startColor[3];
	auto& endColor = data["endColor"];
	endColor_.x = endColor[0];
	endColor_.y = endColor[1];
	endColor_.z = endColor[2];
	endColor_.w = endColor[3];
}