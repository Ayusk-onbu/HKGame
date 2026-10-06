#include "Status.h"
#include "DynamicObject.h"

void StatusComponent::Initialize() {
    currentHp_ = baseHp_;
}

void StatusComponent::Update(float deltaTime) {
    if (IsDead()) return;

    // スリップダメージ（毒・燃焼など）の処理
    for (auto& effect : activeEffects_) {
        if (effect.statusType == StatusType::SlipDamage) {
            currentHp_ -= effect.multiplier * deltaTime;
        }
    }

    // 寿命切れエフェクトの削除
    std::erase_if(activeEffects_, [deltaTime](StatusEffect& effect) {
        effect.duration -= deltaTime;
        return effect.duration <= 0.0f;
        });

    if (currentHp_ <= 0.0f) {
        currentHp_ = 0.0f;
        // master_->OnDeath(); などの死亡時イベント呼出し
    }
}

json StatusComponent::Serialize() const {
    json j;
    j["baseHp"] = baseHp_;
    j["currentHp"] = currentHp_;
    j["baseAttack"] = baseAttack_;
    j["baseDefense"] = baseDefense_;
    j["baseSpeed"] = baseSpeed_;
    j["innateElement"] = static_cast<int>(innateElement_);
    return j;
}

void StatusComponent::Deserialize(const json& j) {
    if (j.contains("baseHp")) baseHp_ = j["baseHp"].get<float>();
    if (j.contains("currentHp")) currentHp_ = j["currentHp"].get<float>();
    if (j.contains("baseAttack")) baseAttack_ = j["baseAttack"].get<float>();
    if (j.contains("baseDefense")) baseDefense_ = j["baseDefense"].get<float>();
    if (j.contains("baseSpeed")) baseSpeed_ = j["baseSpeed"].get<float>();
    if (j.contains("innateElement")) innateElement_ = static_cast<Element>(j["innateElement"].get<int>());
}

void StatusComponent::AddEffect(const StatusEffect& effect) {
    // 例: 即効性の回復やダメージなら、ここでHPを増減させてリストには入れない
        // if (effect.type == StatusType::InstantDamage) { ... return; }

    activeEffects_.push_back(effect);
}

float StatusComponent::GetCurrentAttack() const {
    float totalBonus = 0.0f;
    for (const auto& effect : activeEffects_) {
        if (effect.statusType == StatusType::Attack) {
            totalBonus += effect.multiplier;
        }
    }
    float finalMultiplier = (std::max)(0.1f, 1.0f + totalBonus);
    return baseAttack_ * finalMultiplier;
}

float StatusComponent::GetCurrentDefense() const {
    float totalBonus = 0.0f;
    for (const auto& effect : activeEffects_) {
        if (effect.statusType == StatusType::Defense) {
            totalBonus += effect.multiplier;
        }
    }
    float finalMultiplier = (std::max)(0.1f, 1.0f + totalBonus);
    return baseDefense_ * finalMultiplier;
}

float StatusComponent::GetCurrentSpeed() const {
    float totalBonus = 0.0f;
    for (const auto& effect : activeEffects_) {
        if (effect.statusType == StatusType::Speed) {
            totalBonus += effect.multiplier;
        }
    }
    float finalMultiplier = (std::max)(0.1f, 1.0f + totalBonus);
    return baseSpeed_ * finalMultiplier;
}

float StatusComponent::TakeDamage(float rawDamage, Element attackElement) {
    if (IsDead()) return 0.0f;

    float currentDef = GetCurrentDefense();

    // 相性倍率の取得
    float elementMultiplier = GetElementMultiplier(attackElement, innateElement_);

    // 割り算方式のダメージ計算（防御力100を基準に割合軽減）
    float damageMultiplier = 100.0f / (100.0f + currentDef);
    // もし引き算方式がいい場合はこっち！
    // float actualDamage = (std::max)(1.0f, rawDamage - currentDef);
    // 最終ダメージの計算（基礎威力 × 防御軽減 × 属性相性）
    float actualDamage = rawDamage * damageMultiplier * elementMultiplier;

    // HPを減らす
    currentHp_ -= actualDamage;

    // HPが0を下回らないように補正
    if (currentHp_ <= 0.0f) {
        currentHp_ = 0.0f;
        // ここで OnDeath() のような死んだ時のイベントを呼ぶ
    }

    // ※ここで、もし elementMultiplier > 1.0f なら「弱点エフェクト」や「WEAK!」というイベントを出す

    // 「実際にどれだけ減ったか」を返すことで、攻撃側が「与えたダメージの10%を回復する(吸血)」みたいな処理を書けるようになる
    return actualDamage;
}

float StatusComponent::GetElementMultiplier(Element attackElement, Element defenseElement) const {
    if (attackElement == Element::None || defenseElement == Element::None) {
        return 1.0f;
    }

    if (attackElement == Element::Fire) {
        if (defenseElement == Element::Ice) return 1.5f;
        if (defenseElement == Element::Water) return 0.5f;
    }
    if (attackElement == Element::Water && defenseElement == Element::Fire) return 1.5f;

    if (attackElement == defenseElement) return 0.5f;

    return 1.0f;
}

void StatusComponent::DrawUI() {
#ifdef USE_IMGUI
    if (ImGui::TreeNode("Status Component")) {
        ImGui::ProgressBar(currentHp_ / baseHp_, ImVec2(0.0f, 0.0f), "HP");
        ImGui::Text("HP: %.1f / %.1f", currentHp_, baseHp_);
        ImGui::Text("Attack: %.1f (Base: %.1f)", GetCurrentAttack(), baseAttack_);
        ImGui::Text("Defense: %.1f (Base: %.1f)", GetCurrentDefense(), baseDefense_);
        ImGui::Text("Speed: %.1f (Base: %.1f)", GetCurrentSpeed(), baseSpeed_);

        ImGui::Text("Active Buffs/Debuffs: %zu", activeEffects_.size());

        // デバッグ用：仮のバフ追加ボタン
        if (ImGui::Button("Add Attack Buff (+20%)")) {
            AddEffect({ StatusType::Attack, 0.2f, 5.0f });
        }
        ImGui::SameLine();
        if (ImGui::Button("Take 100 Fire Damage")) {
            TakeDamage(100.0f, Element::Fire);
        }

        ImGui::TreePop();
    }
#endif
}