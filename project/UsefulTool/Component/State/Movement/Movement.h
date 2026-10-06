#pragma once
#include "Component.h"
#include "IMovementAction.h"
#include <vector>
#include <memory>
#include <algorithm>

class DynamicObject;

class MovementComponent : public Component {
public:
    MovementComponent() = default;
    virtual ~MovementComponent() = default;

    void Update(float deltaTime) override;
    void DrawUI() override;
    std::string GetComponentName() const override { return "MovementComponent"; }
    json Serialize() const override;
    void Deserialize(const json& j) override;

    // --- MoveType / Target 設定用のインターフェース ---
    void SetMoveType(MoveType type) { moveType_ = type; }
    MoveType GetMoveType() const { return moveType_; }

    void SetTargetObject(DynamicObject* target) { targetObject_ = target; }
    DynamicObject* GetTargetObject() const { return targetObject_; }

    // --- モジュールの追加 (テンプレートで簡単に登録) ---
    template <typename T, typename... Args>
    T* AddAction(Args&&... args) {
        auto action = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = action.get();
        actions_.push_back(std::move(action));

        // 優先度が高い順 (降順) に自動ソート
        std::sort(actions_.begin(), actions_.end(), [](const auto& a, const auto& b) {
            return a->GetPriority() > b->GetPriority();
            });
        return ptr;
    }

    // --- インデックス指定でのモジュール削除 ---
    void RemoveAction(size_t index) {
        if (index < actions_.size()) {
            if (actions_[index].get() == currentAction_) {
                currentAction_ = nullptr;
            }
            actions_.erase(actions_.begin() + index);
        }
    }

    // 現在実行中のアクション名を取得
    std::string GetCurrentActionName() const {
        return currentAction_ ? currentAction_->GetName() : "None";
    }

private:
    std::vector<std::unique_ptr<IMovementAction>> actions_;
    IMovementAction* currentAction_ = nullptr;

    MoveType moveType_ = MoveType::Camera;
    DynamicObject* targetObject_ = nullptr;
};