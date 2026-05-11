#pragma once
#include "Magic/Magic.h"

class StatusComponent {
public:
    // 外部から魔法などを食らった時に呼ばれる
    void AddEffect(const StatusEffect& effect) {
        // 例: 即効性の回復やダメージなら、ここでHPを増減させてリストには入れない
        // if (effect.type == StatusType::InstantDamage) { ... return; }

        // 持続するバフ・デバフならリストに追加
        activeEffects_.push_back(effect);
    }

    // 毎フレーム呼ばれる（DynamicObjectのUpdate内などで呼ぶ）
    void Update(float deltaTime) {
        // 1. スリップダメージ（毒・燃焼など）の処理
        for (auto& effect : activeEffects_) {
            if (effect.statusType == StatusType::SlipDamage) {
                // 毎フレームちょっとずつダメージを与える（multiplierを秒間ダメージとする）
                currentHp_ -= effect.multiplier * deltaTime;
            }
        }

        // 2. 寿命を減らし、0秒以下のものをリストから一掃する
        std::erase_if(activeEffects_, [deltaTime](StatusEffect& effect) {
            effect.duration -= deltaTime;
            return effect.duration <= 0.0f; // trueなら削除される
            });
    }

    // ==================================================
    // ゲッター（計算時に全部足し算するロマン仕様！）
    // ==================================================
    float GetCurrentAttack() const {
        float totalBonus = 0.0f; // 1.0を基準とせず、ボーナス分を足していく

        for (const auto& effect : activeEffects_) {
            if (effect.statusType == StatusType::Attack) {
                totalBonus += effect.multiplier; // 例: +0.2 (20%UP)
            }
        }

        // 基礎攻撃力 × (1.0 + 0.2 + 0.2 ...) = 重ねるほど強くなる！
        // ※もしデバフで合計がマイナスになったら0.1倍などでストップさせる安全装置
        float finalMultiplier = (std::max)(0.1f, 1.0f + totalBonus);

        return baseAttack_ * finalMultiplier;
    }

    // HP
    float GetCurrentHp() const { return currentHp_; }

    // ==================================================
    // 現在の防御力を取得（攻撃力UPと同じ加算ロマン仕様！）
    // ==================================================
    float GetCurrentDefense() const {
        float totalBonus = 0.0f;
        for (const auto& effect : activeEffects_) {
            if (effect.statusType == StatusType::Defense) {
                totalBonus += effect.multiplier;
            }
        }
        // デバフでマイナスになりすぎないよう、最低でも元の10%の防御力は保証する
        float finalMultiplier = (std::max)(0.1f, 1.0f + totalBonus);
        return baseDefense_ * finalMultiplier;
    }

    // ==================================================
    // ダメージを受ける処理（超重要）
    // 戻り値: 実際に食らった最終ダメージ量（UIでのダメージ数値表示に使える！）
    // ==================================================
    float TakeDamage(float rawDamage, Element attackElement) {
        // すでに死んでいたら何もしない（死体撃ち防止）
        if (currentHp_ <= 0.0f) return 0.0f;

        float currentDef = GetCurrentDefense();

        // 1. 相性倍率の取得
        float elementMultiplier = GetElementMultiplier(attackElement, innateElement_);

        // 2, 割り算方式のダメージ計算（防御力100を基準に割合軽減）
        float damageMultiplier = 100.0f / (100.0f + currentDef);
        // もし引き算方式がいい場合はこっち！
        // float actualDamage = (std::max)(1.0f, rawDamage - currentDef);
        // 3. 最終ダメージの計算（基礎威力 × 防御軽減 × 属性相性）
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

    // 死亡判定ゲッター
    bool IsDead() const { return currentHp_ <= 0.0f; }

    // 自分の属性をセットする用（初期化時に呼ぶ）
    void SetElement(Element elem) { innateElement_ = elem; }
    Element GetElement() const { return innateElement_; }

    // ==================================================
    // 属性の相性倍率を計算する便利関数（ここは好きに設定！）
    // ==================================================
    float GetElementMultiplier(Element attackElement, Element defenseElement) const {
        if (attackElement == Element::None || defenseElement == Element::None) {
            return 1.0f; // 無属性が絡む場合は等倍
        }

        // 例: 炎は氷に強く、水に弱い
        if (attackElement == Element::Fire) {
            if (defenseElement == Element::Ice) return 1.5f;   // 弱点（Weak!）
            if (defenseElement == Element::Water) return 0.5f; // 耐性（Resist!）
        }
        // 例: 水は炎に強い
        if (attackElement == Element::Water && defenseElement == Element::Fire) return 1.5f;

        // 同属性への攻撃は半減
        if (attackElement == defenseElement) return 0.5f;

        return 1.0f; // それ以外は等倍
    }

	void SetCurrentHp(float hp) { currentHp_ = hp; }

    float GetCurrentSpeed() const;

private:
    // --- 基礎ステータス ---
    float baseHp_ = 1000.0f;
    float currentHp_ = 1000.0f;
    float baseAttack_ = 100.0f;
    float baseDefense_ = 50.0f;
    float baseSpeed_ = 5.0f;

	std::vector<StatusEffect> activeEffects_;

    Element innateElement_ = Element::None; // ← 自分の属性（敵なら炎属性モンスター等）
};