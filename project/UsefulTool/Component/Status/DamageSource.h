#pragma once
#include "Component.h"
#include "GameplayTypes.h"
#include <functional>
#include <vector>

class DynamicObject;
class StatusComponent;

// ダメージ算出の計算タイプ
enum class DamageCalcType {
    SelfOnly,        // ① 自身（このオブジェクト）のATKのみ
    OwnerOnly,       // ② 持ち主（Owner）のATKのみ
    OwnerAndWeapon,  // ③ 持ち主のATK + 自身のATK（合成）
    PercentMaxHP,    // ④ 相手の最大HP割合（例: 0.1f なら 10% ダメージ）
    Custom           // ⑥ 任意のラムダ式で計算
};

class DamageSourceComponent : public Component {
public:
    DamageSourceComponent() = default;
    virtual ~DamageSourceComponent() = default;

    void Initialize() override;
    void DrawUI()override;
    std::string GetComponentName() const override { return "DamageSourceComponent"; }
    json Serialize() const override;
    void Deserialize(const json& j) override;

    // --- 攻撃パラメータの設定 ---
    void SetCalcType(DamageCalcType type) { calcType_ = type; }
    void SetBasePower(float power) { basePower_ = power; }
    void SetElement(Element elem) { element_ = elem; }
    void SetOwner(DynamicObject* owner) { owner_ = owner; }

    // 状況依存（⑤）のためのモーション倍率・チャージ倍率など
    void SetMotionMultiplier(float mult) { motionMultiplier_ = mult; }

    // カスタム計算式（⑥）の登録
    void SetCustomCalc(std::function<float(StatusComponent* myStatus, StatusComponent* targetStatus)> func) {
        customCalc_ = func;
        calcType_ = DamageCalcType::Custom;
    }

    // バフ・デバフ効果の追加
    void AddStatusEffect(const StatusEffect& effect) { statusEffects_.push_back(effect); }
    const std::vector<StatusEffect>& GetStatusEffects() const { return statusEffects_; }

    // --- 最終ダメージの計算処理（コアロジック） ---
    float CalculateRawDamage(StatusComponent* targetStatus);

    Element GetElement() const { return element_; }

private:
    DamageCalcType calcType_ = DamageCalcType::OwnerAndWeapon;

    float basePower_ = 50.0f;          // 武器単体の攻撃力、または罠の固定ダメージ
    float motionMultiplier_ = 1.0f;    // 技のモーション倍率（例: 強攻撃なら 1.5f）
    Element element_ = Element::None;

    DynamicObject* owner_ = nullptr;   // 持ち主（プレイヤーや敵など）
    std::vector<StatusEffect> statusEffects_; // 付与する状態異常

    // カスタム計算用
    std::function<float(StatusComponent*, StatusComponent*)> customCalc_ = nullptr;
};