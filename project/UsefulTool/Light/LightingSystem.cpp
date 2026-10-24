#include "LightingSystem.h"
#include "Fngine.h"

void LightingSystem::Initialize(Fngine* engine) {
	p_engine_ = engine;

	// 作成
	//directionalLights_ = std::make_unique<DirectionalLightManager>(p_engine_);

	// 初期化
	//directionalLights_->Initialize(3);
	//directionalLights_->AddLight();
}