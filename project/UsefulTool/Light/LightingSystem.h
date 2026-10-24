#pragma once
#include "DirectionLight.h"
#include "ISingleton.h"

class Fngine;

class LightingSystem :
	public ISingleton<LightingSystem>
{
public:
	friend class ISingleton<LightingSystem>;

public:

	void Initialize(Fngine* engine);

public:
	//   ==========
	// 【 Get 関係 】
	//   ==========

	// 並行光源
	//DirectionalLightManager* GetDirectionalLights() { return directionalLights_.get(); }

	// 随時追加予定

private:
	Fngine* p_engine_;

	//std::unique_ptr<DirectionalLightManager>directionalLights_;
};
