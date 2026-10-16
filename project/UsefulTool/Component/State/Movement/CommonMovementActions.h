#pragma once
#include "IMovementAction.h"
#include "DynamicObject.h"
#include "../../Physics/RigidBody.h"
#include "../../Status/Status.h"

// -------------------------------------------------------------
// ① 待機モジュール (Idle) - 優先度: 0 (最低・デフォルト)
// -------------------------------------------------------------
class IdleMovementAction : public IMovementAction {
public:
    bool CanExecute(DynamicObject* owner, const CommandState& input) const override {
        // 地面にいて、移動入力が無い場合
        auto* rb = owner->GetComponent<RigidBody>();
        return rb && rb->IsOnGround() && (LengthSquared(input.moveDirection) <= 0.001f);
    }

    void OnUpdate(DynamicObject* owner, const CommandState& input, float deltaTime) override {
        auto* rb = owner->GetComponent<RigidBody>();
        if (!rb) return;

        // XZ平面の速度を摩擦で減衰させる
        Vector3 vel = rb->GetVelocity();
        vel.x *= 0.8f;
        vel.z *= 0.8f;
        rb->SetVelocity(vel);
    }

    int GetPriority() const override { return 0; }
    const std::string& GetName() const override { static std::string name = "Idle"; return name; }
};

// -------------------------------------------------------------
// ② 歩き/走りモジュール (Walk) - 優先度: 10
// -------------------------------------------------------------
class WalkMovementAction : public IMovementAction {
public:
    bool CanExecute(DynamicObject* owner, const CommandState& input) const override {
        auto* rb = owner->GetComponent<RigidBody>();
        // 地面にいて、移動入力がある場合
        return rb && rb->IsOnGround() && (LengthSquared(input.moveDirection) > 0.001f);
    }

    void OnUpdate(DynamicObject* owner, const CommandState& input, float deltaTime) override {
        auto* rb = owner->GetComponent<RigidBody>();
        auto* status = owner->GetComponent<StatusComponent>();
        if (!rb) return;

        float speed = status ? status->GetCurrentSpeed() : 5.0f;

        // 入力方向をカメラ基準等に補正して速度に反映
        Vector3 vel = rb->GetVelocity();
        vel.x = input.moveDirection.x * speed;
        vel.z = input.moveDirection.z * speed;
        rb->SetVelocity(vel);
    }

    int GetPriority() const override { return 10; }
    const std::string& GetName() const override { static std::string name = "Walk"; return name; }
};

// -------------------------------------------------------------
// ③ ジャンプモジュール (Jump) - 優先度: 50
// -------------------------------------------------------------
class JumpMovementAction : public IMovementAction {
public:
    JumpMovementAction(float jumpPower = 8.0f) : jumpPower_(jumpPower) {}

    bool CanExecute(DynamicObject* owner, const CommandState& input) const override {
        auto* rb = owner->GetComponent<RigidBody>();
        // 「ジャンプボタン押下」かつ「接地中（またはコヨーテタイム中）」
        bool buttonPressed = (input.subMove == ButtonState::Pressed);
        bool canJump = rb && (rb->IsOnGround() || coyoteTimer_ > 0.0f);
        return buttonPressed && canJump;
    }

    void OnEnter(DynamicObject* owner) override {
        auto* rb = owner->GetComponent<RigidBody>();
        if (!rb) return;

        // Y方向に一気に初速を与える！
        Vector3 vel = rb->GetVelocity();
        vel.y = jumpPower_;
        //rb->SetVelocity(vel);
        rb->SetExternalVelocity(vel);

        coyoteTimer_ = 0.0f; // ジャンプしたので猶予消化
    }

    void OnUpdate(DynamicObject* owner, const CommandState& input, float deltaTime) override {
        // 空中移動の慣性適用など
    }

    int GetPriority() const override { return 50; }
    const std::string& GetName() const override { static std::string name = "Jump"; return name; }

    void SetCoyoteTime(float time) { coyoteTimer_ = time; }

private:
    float jumpPower_ = 9.0f;
    mutable float coyoteTimer_ = 0.0f;
};

// -------------------------------------------------------------
// ④ 空中移動/落下モジュール (Air) - 優先度: 5
// -------------------------------------------------------------
class AirMovementAction : public IMovementAction {
public:
    bool CanExecute(DynamicObject* owner, const CommandState& input) const override {
        auto* rb = owner->GetComponent<RigidBody>();
        // 地面に触れていない場合は強制的に空中状態！
        return rb && !rb->IsOnGround();
    }

    void OnUpdate(DynamicObject* owner, const CommandState& input, float deltaTime) override {
        auto* rb = owner->GetComponent<RigidBody>();
        auto* status = owner->GetComponent<StatusComponent>();
        if (!rb) return;

        // 空中でも少しだけハンドル操作（慣性移動）できるようにする
        float speed = status ? status->GetCurrentSpeed() * 0.5f : 2.5f;
        Vector3 vel = rb->GetVelocity();

        if (LengthSquared(input.moveDirection) > 0.001f) {
            vel.x = input.moveDirection.x * speed;
            vel.z = input.moveDirection.z * speed;
        }
        rb->SetVelocity(vel);
    }

    int GetPriority() const override { return 5; }
    const std::string& GetName() const override { static std::string name = "Air"; return name; }
};

class SplineFollowMovementAction : public IMovementAction {
public:
    bool CanExecute(DynamicObject* owner, const CommandState& input) const override {
        // 移動入力が存在していれば実行可能
        return LengthSquared(input.moveDirection) > 0.001f;
    }

    void OnUpdate(DynamicObject* owner, const CommandState& input, float deltaTime) override {
        auto* rb = owner->GetComponent<RigidBody>();
        auto* status = owner->GetComponent<StatusComponent>();
        if (!rb) return;

        float speed = status ? status->GetCurrentSpeed() : 4.0f;

        // スプライン脳みそ（Controller）が出力した方向へ剛体を動かす
        Vector3 vel = rb->GetVelocity();
        vel.x = input.moveDirection.x * speed;
        vel.z = input.moveDirection.z * speed;
        rb->SetVelocity(vel);
    }

    int GetPriority() const override { return 20; } // 通常のWalk(10)より優先
    const std::string& GetName() const override { static std::string name = "SplineFollow"; return name; }
};