#pragma once
#include <vector>
#include <string>

// --- 誰を対象にするか ---
enum class TargetType {
    Enemy,      // 敵（ホーミング弾やターゲット指定）
    Self,       // 自分（バフや回復など）
    Area        // 指定座標・周囲（設置魔法など）
};

// --- どうやって発動するか（挙動） ---
enum class BehaviorType {
    Projectile, // 弾を飛ばす（スプライン軌道を使う）
    Instant,    // 即時発動（自分へのバフや、即着弾の雷など）
    Aura        // 自分の周囲に展開して追従する
};

enum class StatusType {
    Attack,          // 攻撃力
    Defense,         // 防御力
    Speed,           // 移動速度
    MaxHealth,       // 最大HP（バフ用）
    SlipDamage,      // 毒や燃焼などのスリップダメージ
    Stun             // スタン（麻痺・行動不能）
};

// --- バフ・デバフの詳細データ ---
struct StatusEffect {
    StatusType statusType;   // 何を変化させるか
    float multiplier; // 倍率（例: 1.2f なら20%アップ、0.8f なら20%ダウン）
    float duration;   // 効果時間（秒）
};

// --- 属性の定義 ---
enum class Element {
    None,       // 無属性
    Fire,       // 炎
    Water,      // 水
    Ice,        // 氷
    Wind,       // 風
    Lightning,  // 雷
    Earth,      // 土
    Light,      // 光
    Dark        // 闇
};

// --- 弾1発が知るべき情報（ProjectileData） ---
struct ProjectileData {
    // --- 効果（Effect）データ ---
    float power;            // 基本威力（回復量にも流用）
    std::vector<StatusEffect> statusEffects; // バフ・デバフ効果（複数設定可）

    Element element;        // 属性
    TargetType target;
    
    std::string splineName;
    bool isHoming;
    bool isPiercing; // 貫通するかどうか（ヒットリストに関係）
};

// ==========================================
// 究極の MagicData
// ==========================================
struct MagicData {
    int magicId;            // 魔法の識別ID
    float castTime;         // 詠唱時間
    float cooldownTime;     // クールタイム
    BehaviorType behavior;

    // --- 弾（Projectile）専用データ ---
    int projectileCount;
    float spawnInterval;
    std::vector<std::string> splineJsonPaths; // 複数の軌道
    float splineDuration;

    // 弾を生み出す時は、このデータを渡す！
    ProjectileData projData;
};

