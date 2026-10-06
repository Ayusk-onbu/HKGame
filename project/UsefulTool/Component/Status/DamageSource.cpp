#include "DamageSource.h"
#include "DynamicObject.h"
#include "Status.h"

void DamageSourceComponent::Initialize() {
    // 初期化処理（必要に応じて）
}

json DamageSourceComponent::Serialize() const {
    json j;
    j["calcType"] = static_cast<int>(calcType_);
    j["basePower"] = basePower_;
    j["motionMultiplier"] = motionMultiplier_;
    j["element"] = static_cast<int>(element_);

    json effects = json::array();
    for (const auto& effect : statusEffects_) {
        json e;
        e["statusType"] = static_cast<int>(effect.statusType);
        e["multiplier"] = effect.multiplier;
        e["duration"] = effect.duration;
        effects.push_back(e);
    }
    j["statusEffects"] = effects;
    return j;
}

void DamageSourceComponent::Deserialize(const json& j) {
    if (j.contains("calcType")) calcType_ = static_cast<DamageCalcType>(j["calcType"].get<int>());
    if (j.contains("basePower")) basePower_ = j["basePower"].get<float>();
    if (j.contains("motionMultiplier")) motionMultiplier_ = j["motionMultiplier"].get<float>();
    if (j.contains("element")) element_ = static_cast<Element>(j["element"].get<int>());

    statusEffects_.clear();
    if (j.contains("statusEffects") && j["statusEffects"].is_array()) {
        for (const auto& e : j["statusEffects"]) {
            StatusEffect effect{};
            if (e.contains("statusType")) effect.statusType = static_cast<StatusType>(e["statusType"].get<int>());
            if (e.contains("multiplier")) effect.multiplier = e["multiplier"].get<float>();
            if (e.contains("duration")) effect.duration = e["duration"].get<float>();
            statusEffects_.push_back(effect);
        }
    }
}

void DamageSourceComponent::DrawUI() {
#ifdef USE_IMGUI
    if (ImGui::TreeNode("Damage Source Component")) {

        // 1. 計算タイプ (DamageCalcType) のドロップダウン
        const char* calcTypeNames[] = {
            "Self Only (Self ATK + Skill Power)",
            "Owner Only (Owner ATK)",
            "Owner & Weapon (Owner ATK + Skill Power)",
            "Percent Max HP (Target Max HP %)",
            "Custom (Lambda Formula)"
        };
        int currentType = static_cast<int>(calcType_);
        if (ImGui::Combo("Calc Type", &currentType, calcTypeNames, IM_ARRAYSIZE(calcTypeNames))) {
            calcType_ = static_cast<DamageCalcType>(currentType);
        }

        // 2. 威力 (Base Power) & モーション倍率 (Motion Multiplier)
        ImGui::DragFloat("Base Power", &basePower_, 1.0f, 0.0f, 9999.0f, "%.1f");
        ImGui::DragFloat("Motion Multiplier", &motionMultiplier_, 0.05f, 0.0f, 10.0f, "%.2f");

        // 3. 属性 (Element) の選択
        const char* elementNames[] = {
            "None", "Fire", "Water", "Ice", "Wind", "Lightning", "Earth", "Light", "Dark"
        };
        int currentElement = static_cast<int>(element_);
        if (ImGui::Combo("Element", &currentElement, elementNames, IM_ARRAYSIZE(elementNames))) {
            element_ = static_cast<Element>(currentElement);
        }

        // 4. 持ち主 (Owner) の状態表示
        if (owner_) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Owner: Attached (DynamicObject)");
        }
        else {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Owner: None (Self / World Trap)");
        }

        ImGui::Separator();

        // 5. 付与するバフ・デバフ（StatusEffects）の管理
        if (ImGui::TreeNode("Attached Status Effects")) {
            ImGui::Text("Effects Count: %zu", statusEffects_.size());

            for (size_t i = 0; i < statusEffects_.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                auto& effect = statusEffects_[i];

                const char* statusTypeNames[] = { "Attack", "Defense", "Speed", "MaxHealth", "SlipDamage", "Stun" };
                int typeIdx = static_cast<int>(effect.statusType);

                ImGui::Text("[%zu] Target: %s", i, statusTypeNames[typeIdx]);
                ImGui::SameLine();
                if (ImGui::Button("Remove")) {
                    statusEffects_.erase(statusEffects_.begin() + i);
                    ImGui::PopID();
                    break;
                }

                ImGui::DragFloat("Value/Multiplier", &effect.multiplier, 0.05f, -5.0f, 5.0f, "%.2f");
                ImGui::DragFloat("Duration (sec)", &effect.duration, 0.1f, 0.0f, 60.0f, "%.1f");
                ImGui::Separator();

                ImGui::PopID();
            }

            // デバッグ追加ボタン
            if (ImGui::Button("+ Add Burn (Slip Damage)")) {
                AddStatusEffect({ StatusType::SlipDamage, 10.0f, 3.0f });
            }
            ImGui::SameLine();
            if (ImGui::Button("+ Add ATK Debuff (-20%)")) {
                AddStatusEffect({ StatusType::Attack, -0.2f, 5.0f });
            }

            ImGui::TreePop();
        }

        ImGui::TreePop();
    }
#endif
}

float DamageSourceComponent::CalculateRawDamage(StatusComponent* targetStatus) {
    float finalAtk = 0.0f;

    // 持ち主の StatusComponent を取得
    StatusComponent* ownerStatus = nullptr;
    if (owner_) {
        ownerStatus = owner_->GetComponent<StatusComponent>();
    }

    // 自身の StatusComponent を取得（自身がステータスを持つ場合）
    StatusComponent* myStatus = master_ ? master_->GetComponent<StatusComponent>() : nullptr;

    switch (calcType_) {
    case DamageCalcType::SelfOnly:
        // ① 自身完結（自身のステータス参照、無ければ basePower_）
        if (myStatus) {
            finalAtk = myStatus->GetCurrentAttack() + basePower_;
        }
        else {
            finalAtk = basePower_;
        }
        break;

    case DamageCalcType::OwnerOnly:
        // ② 持ち主依存（素手や魔法など）
        if (ownerStatus) {
            finalAtk = ownerStatus->GetCurrentAttack();
        }
        else {
            finalAtk = basePower_; // フォールバック
        }
        break;

    case DamageCalcType::OwnerAndWeapon:
        // ③ 持ち主 + 武器の合成（定番の装備品）
    {
        float ownerAtk = ownerStatus ? ownerStatus->GetCurrentAttack() : 0.0f;
        finalAtk = ownerAtk + basePower_;
    }
    break;

    case DamageCalcType::PercentMaxHP:
        // ④ 相手の最大HP割合（割合トラップや毒など）
        if (targetStatus) {
            // basePower_ を割合（例: 0.1f = 10%）として扱う
            finalAtk = targetStatus->GetMaxHp() * basePower_;
        }
        break;

    case DamageCalcType::Custom:
        // ⑥ カスタム計算式
        if (customCalc_) {
            finalAtk = customCalc_(myStatus ? myStatus : ownerStatus, targetStatus);
        }
        break;
    }

    // ⑤ モーション倍率やチャージ倍率を乗算して最終攻撃力を算出！
    return finalAtk * motionMultiplier_;
}