#pragma once
#include "Component.h"
#include "InputHandler.h"

class ControllerComponent : public Component {
public:
    ControllerComponent() = default;

    void Initialize() override {}
    void Update(float deltaTime) override {
        if (controller_) {
            // 毎フレーム「脳みそ」に次の入力コマンド（CommandState）を計算させる
            commandState_ = controller_->GetCommandState(commandState_);
        }
    }
    void DrawUI() override;
    std::string GetComponentName() const override { return "ControllerComponent"; }
    json Serialize() const override;
    void Deserialize(const json& j) override;

    // 脳みその差し替え（PlayerController, HermiteSplineController, AIController 等）
    void SetController(std::unique_ptr<Controller> ctrl, const std::string& name = "Custom") {
        controller_ = std::move(ctrl);
        controllerName_ = name;
    }

    Controller* GetController() const { return controller_.get(); }
    const CommandState& GetCommand() const { return commandState_; }
    const std::string& GetControllerName() const { return controllerName_; }
private:
    std::unique_ptr<Controller> controller_ = nullptr;
    CommandState commandState_{};
    std::string controllerName_ = "None";
};