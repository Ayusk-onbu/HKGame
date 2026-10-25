#pragma once
#include "Component.h"

// 物理挙動のタイプ
enum class BodyType {
    Dynamic,   // 完全物理（重力・力学で動く）
    Kinematic, // スクリプト制御（速度直接指定・着地や壁押し戻しのみ適用）
    Static     // 動かない（壁・床）
};

class RigidBody : public Component {
public:
    RigidBody() = default;
    virtual ~RigidBody() = default;

    void Initialize() override;
    void Update(float deltaTime) override;
    void DrawUI()override;
    // コンポーネントの種類名
    std::string GetComponentName()const override;
    // jsonに値を保存
    json Serialize()const override;
    // jsonから値を取得
    void Deserialize(const json& j)override;

public:

    // 外力を加える（Dynamic用）
    void AddForce(const Vector3& force);

    // 衝突時のめり込み補正処理（ColliderComponentのコールバックから呼ぶ）
    void ResolveCollision(const Vector3& pushOut);

    // --- ゲッター・セッター ---
    void SetBodyType(BodyType type) { bodyType_ = type; }
    BodyType GetBodyType() const { return bodyType_; }

    // 自身の意思/入力による速度の設定・取得
    void SetVelocity(const Vector3& vel) { myVelocity_ = vel; }
    const Vector3& GetVelocity() const { return myVelocity_; }

    // 外部から加わる速度（動く床、風、爆風など）の設定・加算
    void SetExternalVelocity(const Vector3& extVel) { externalVelocity_ = extVel; }
    void AddExternalVelocity(const Vector3& extVel) { externalVelocity_ += extVel; }
    const Vector3& GetExternalVelocity() const { return externalVelocity_; }

    // 合計速度（内部速度 ＋ 外部速度）を取得
    Vector3 GetTotalVelocity() const { return myVelocity_ + externalVelocity_; }

    void SetUseGravity(bool use) { useGravity_ = use; }
    bool IsOnGround() const { return isOnGround_; }

    void SetMass(float mass) { mass_ = (mass > 0.0f) ? mass : 1.0f; }
    void SetRestitution(float val) { restitution_ = val; } // 反発係数 (0.0〜1.0)
    void SetFriction(float val) { friction_ = val; }       // 摩擦係数 (0.0〜1.0)

    // 進行方向に向かせるかどうかのフラグ
    void SetOrientToMovement(bool enable) { orientToMovement_ = enable; }

private:
    BodyType bodyType_ = BodyType::Static;

    Vector3 myVelocity_{ 0, 0, 0 };       // 自身の入力や意思による速度
    Vector3 externalVelocity_{ 0, 0, 0 }; // 外部環境からの速度
    Vector3 force_Accumulator_{ 0, 0, 0 }; // 1フレーム内に加わった力の合計

    bool orientToMovement_ = true;       // 移動方向に向くかどうかの設定
    bool useGravity_ = true;
    bool isOnGround_ = false;            // 着地フラグ

    float mass_ = 1.0f;
    float restitution_ = 0.0f; // 跳ね返り度合い（0=弾まない, 1=完全反発）
    float friction_ = 0.2f;    // 減速の強さ

    const Vector3 kGravity = { 0.0f, -9.81f * 10.0f, 0.0f }; // 重力加速度
};