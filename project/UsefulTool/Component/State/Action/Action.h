#pragma once
#include "Component.h"
#include "ActionStateBase.h"
#include <memory>
#include <string>

class ActionComponent : public Component {
public:
    ActionComponent() = default;
    virtual ~ActionComponent() = default;

    void Update(float deltaTime) override;
    void DrawUI() override;
    std::string GetComponentName() const override { return "ActionComponent"; }
    json Serialize() const override;
    void Deserialize(const json& j) override;

    // --- アクションの切り替え ---
    template <typename TState, typename... Args>
    void ChangeState(Args&&... args) {
        if (currentState_) {
            currentState_->OnExit(master_);
        }

        auto newState = std::make_unique<TState>();
        currentState_ = std::move(newState);

        if (currentState_) {
            currentState_->OnEnter(master_);
        }
    }

    // 移動許可チェック（MovementComponent から参照される）
    bool IsMovementAllowed() const {
        return currentState_ ? currentState_->IsMovementAllowed() : true;
    }

    float GetSpeedMultiplier() const {
        return currentState_ ? currentState_->GetSpeedMultiplier() : 1.0f;
    }

    std::string GetCurrentActionName() const {
        return currentState_ ? currentState_->GetName() : "None/Free";
    }

private:
    std::unique_ptr<ActionStateBase> currentState_ = nullptr;
};