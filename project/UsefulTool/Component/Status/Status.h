#pragma once
#include "Component.h"
#include "GameplayTypes.h"
#include <vector>

class StatusComponent : public Component {
public:
    StatusComponent() = default;
    virtual ~StatusComponent() = default;

    void Initialize() override;
    void Update(float deltaTime) override;
    void DrawUI() override;
    std::string GetComponentName() const override { return "StatusComponent"; }
    json Serialize() const override;
    void Deserialize(const json& j) override;
	
    // --- 効果の追加 ---
    void AddEffect(const StatusEffect& effect);

    // --- ダメージ処理 ---
    float TakeDamage(float rawDamage, Element attackElement);

    // --- ゲッター / セッター ---
    float GetCurrentHp() const { return currentHp_; }
    float GetMaxHp() const { return baseHp_; }
    float GetCurrentAttack() const;
    float GetCurrentDefense() const;
    float GetCurrentSpeed() const;
    bool IsDead() const { return currentHp_ <= 0.0f; }

    void SetElement(Element elem) { innateElement_ = elem; }
    Element GetElement() const { return innateElement_; }

    void SetBaseStats(float hp, float atk, float def, float spd) {
        baseHp_ = hp;
        currentHp_ = hp;
        baseAttack_ = atk;
        baseDefense_ = def;
        baseSpeed_ = spd;
    }

private:
    float GetElementMultiplier(Element attackElement, Element defenseElement) const;

private:
    float baseHp_ = 1000.0f;
    float currentHp_ = 1000.0f;
    float baseAttack_ = 100.0f;
    float baseDefense_ = 50.0f;
    float baseSpeed_ = 5.0f;

    std::vector<StatusEffect> activeEffects_;
    Element innateElement_ = Element::None;
};