export module StatusComponent;

import <algorithm>;

export class StatusComponent {
public:
    // コンストラクタ（初期ステータスを設定）
    StatusComponent(float maxHp, float attack, float defense)
        : maxHp_(maxHp), currentHp_(maxHp), attack_(attack), defense_(defense) {
    }

    ~StatusComponent() = default;
public:

    ///////////////////////////////////
    //   アクション（ダメージ・回復）
    ///////////////////////////////////

    // ダメージを受ける処理
    void TakeDamage(float incomingDamage);

    // 回復する処理
    void Heal(float amount);

    // 生死判定
    bool IsDead() const { return currentHp_ <= 0.0f; }

    ///////////////////////////////////
    //   Get・Set関係
    ///////////////////////////////////

    float GetHp() const { return currentHp_; }
    float GetMaxHp() const { return maxHp_; }
    float GetAttack() const { return attack_; }
    float GetDefense() const { return defense_; }

    // ※マナ消費で攻撃力を上げたりする時に使う
    void SetAttack(float attack) { attack_ = attack; }
    void SetDefense(float defense) { defense_ = defense; }
private:
	float maxHp_;
	float currentHp_;
	float attack_;
	float defense_;
};