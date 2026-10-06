#include "Action.h"
#include "DynamicObject.h"
#include "../../Controller/Controller.h"

void ActionComponent::Update(float deltaTime) {
    if (!master_) return;

    // 脳みそ (ControllerComponent) から最新コマンドを取得
    CommandState input{};
    if (auto* ctrl = master_->GetComponent<ControllerComponent>()) {
        input = ctrl->GetCommand();
    }

    // 現在のアクションを更新
    if (currentState_) {
        currentState_->OnUpdate(master_, input, deltaTime);
    }
}

json ActionComponent::Serialize() const {
    json j;
    j["currentActionName"] = GetCurrentActionName();
    return j;
}

void ActionComponent::Deserialize(const json& j) {
    // 必要に応じて状態のロード処理を記述
}

void ActionComponent::DrawUI() {
#ifdef USE_IMGUI
    if (ImGui::TreeNode("Action Component")) {
        ImGui::Text("Current Action: %s", GetCurrentActionName().c_str());
        ImGui::Text("Movement Allowed: %s", IsMovementAllowed() ? "YES" : "NO");
        ImGui::Text("Speed Multiplier: %.2f", GetSpeedMultiplier());
        ImGui::TreePop();
    }
#endif
}