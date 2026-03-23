#pragma once
#include "Scene.h"
#include "Collider.h"
#include "Quaternion.h"
#include "Particle.h"
#include "UIContainer.h"
#include "Ground.h"
#include "../TD3Game/Player/Player.h"

import Hermite;
using namespace MathUtils;

class TestScene
	: public Scene
{
public:
	TestScene() = default;
	~TestScene()override;
public:

	// =================================
	// Override Functions
	// =================================

	void Initialize() override;
	void Update() override;
	void Draw() override;
	void PauseUpdate()override;
	void PauseDraw()override;
	bool CanPause()const override { return true; }

private:
	UIContainer container_;
	UIHAnimation animation_;

	std::unique_ptr<Particle>particle_;

	std::unique_ptr<Player>testPlayer_;

	std::unique_ptr<ConvenienceModel>ground_;
};

