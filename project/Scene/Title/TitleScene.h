#pragma once
#include "Scene.h"
#include "Collider.h"
#include "Quaternion.h"
#include "Particle.h"
#include "UIContainer.h"
#include "Ground.h"

class TitleScene
	:public Scene
{
public:
	TitleScene() = default;
	~TitleScene()override;
public:
	void Initialize()override;
	void Update()override;
	void Draw()override;
private:
	float toGameTimer_ = 0.0f;
	std::unique_ptr<UIContainer>titles_;
	std::unique_ptr<SpriteObject>fade_;
};