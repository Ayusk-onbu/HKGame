module;
#include "ImGuiManager.h"
#include "MathUtils.h"
module ManaComponent;

import <algorithm>;

void ManaComponent::Update(float deltaTime) {
	float manaDuration = 15.0f;
	currentMana_ += deltaTime * manaDuration;
	
	// 上限を設定
	currentMana_ = currentMana_ > maxMana_ ? maxMana_ : currentMana_;
}