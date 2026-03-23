export module ManaComponent;

import <algorithm>;

export class ManaComponent {
public:
	ManaComponent(float maxMana = 100.0f) : maxMana_(maxMana), currentMana_(0.0f) {}
public:
	// マナの基本的な処理
	void Update(float deltaTime);
	// マナの増減
	void AddMana(float amount) {
		currentMana_ += amount;

		// std::clamp で 0 ～ maxMana の間に数値を安全に収める（上限突破を防ぐ）
		currentMana_ = std::clamp(currentMana_, 0.0f, maxMana_);
	}
	bool HasEnoughMana(float cost) const { return currentMana_ >= cost; }
	bool ConsumeMana(float cost) {
		if (HasEnoughMana(cost)) {
			currentMana_ -= cost;
			return true;
		}
		return false; // マナ不足
	}

private:
	float currentMana_;
	float maxMana_;
public:
	// ゲッター
	float GetCurrentMana() const { return currentMana_; }
	float GetMaxMana() const { return maxMana_; }
};