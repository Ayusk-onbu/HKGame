#include "Movement.h"
#include "DynamicObject.h"
#include "../../Controller/Controller.h"
#include "CommonMovementActions.h"

void MovementComponent::Update(float deltaTime) {
    if (!master_) return;

    // 脳みそ (ControllerComponent) から最新コマンドを取得 (無ければ空コマンド)
    CommandState input{};
    if (auto* ctrl = master_->GetComponent<ControllerComponent>()) {
        input = ctrl->GetCommand();
    }

    // MoveDirectionを変換
	Vector3 playerPos = master_->GetTransform().get_.Translation();
    std::optional<Vector3> targetPos = std::nullopt;

    if (targetObject_) {
		targetPos = targetObject_->GetTransform().get_.Translation();
    }

    // 変換
	input.moveDirection = CalculateMoveVector(
        input.moveDirection, 
        moveType_, 
        playerPos, 
        targetPos
    );

    if (moveType_ == MoveType::LockOn && targetPos.has_value()) {
        Vector3 toTarget = targetPos.value() - playerPos;
        toTarget.y = 0.0f;
        if (LengthSquared(toTarget) > 0.0001f) {
            master_->GetTransform().LookAtToDirection(Normalize(toTarget));
        }
    }

    // 優先度順に CanExecute を評価し、最初に true になったアクションを選択
    IMovementAction* bestAction = nullptr;
    for (auto& action : actions_) {
        if (action->CanExecute(master_, input)) {
            bestAction = action.get();
            break; // 優先度順なので、最初に見つかったものが最優先！
        }
    }

    // アクションの切り替え処理 (Exit ➔ Enter)
    if (currentAction_ != bestAction) {
        if (currentAction_) currentAction_->OnExit(master_);
        currentAction_ = bestAction;
        if (currentAction_) currentAction_->OnEnter(master_);
    }

    // 現在のアクションを更新
    if (currentAction_) {
        currentAction_->OnUpdate(master_, input, deltaTime);
    }
}

json MovementComponent::Serialize() const {
    json j;
    j["moveType"] = static_cast<int>(moveType_);

    json actionList = json::array();
    for (const auto& action : actions_) {
        actionList.push_back(action->GetName());
    }
    j["actions"] = actionList;
    return j;
}

void MovementComponent::Deserialize(const json& j) {
    if (j.contains("moveType")) moveType_ = static_cast<MoveType>(j["moveType"].get<int>());

    actions_.clear();
    currentAction_ = nullptr;
    if (j.contains("actions") && j["actions"].is_array()) {
        for (const auto& nameJson : j["actions"]) {
            std::string name = nameJson.get<std::string>();
            if (name == "Idle") AddAction<IdleMovementAction>();
            else if (name == "Walk") AddAction<WalkMovementAction>();
            else if (name == "Jump") AddAction<JumpMovementAction>();
            else if (name == "Air") AddAction<AirMovementAction>();
            else if (name == "SplineFollow") AddAction<SplineFollowMovementAction>();
        }
    }
}

void MovementComponent::DrawUI() {
#ifdef USE_IMGUI
    if (ImGui::TreeNode("Movement Component")) {
        ImGui::Text("Active Action: %s", GetCurrentActionName().c_str());
        ImGui::Separator();

        // ---------------------------------------------------
        // MoveType切り替え
        // ---------------------------------------------------
        const char* moveTypeNames[] = { "Raw", "Camera", "Reverse", "Screen", "LockOn" };
        int currentType = static_cast<int>(moveType_);
        if (ImGui::Combo("Move Type", &currentType, moveTypeNames, IM_ARRAYSIZE(moveTypeNames))) {
            moveType_ = static_cast<MoveType>(currentType);
        }

        ImGui::Separator();


        // ---------------------------------------------------
        // アタッチ済み Action 一覧 & 削除ボタン
        // ---------------------------------------------------
        ImGui::Text("Attached Actions (%zu):", actions_.size());

        for (size_t i = 0; i < actions_.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));

            bool isActive = (actions_[i].get() == currentAction_);

            // 優先度と名前を表示
            ImGui::Text("[%d] %s", actions_[i]->GetPriority(), actions_[i]->GetName().c_str());

            if (isActive) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "(ACTIVE)");
            }

            ImGui::SameLine();
            if (ImGui::Button("Remove")) {
                RemoveAction(i);
                ImGui::PopID();
                break;
            }

            ImGui::PopID();
        }

        ImGui::Separator();

        // ---------------------------------------------------
        // UIからの新規 Action アタッチ
        // ---------------------------------------------------
        const char* availableActions[] = {
            "Idle (IdleMovementAction)",
            "Walk (WalkMovementAction)",
            "Jump (JumpMovementAction)",
            "Air (AirMovementAction)",
            "SplineFollow (SplineFollowMovementAction)"
        };
        static int selectedActionIndex = 0;

        ImGui::Combo("Select Action", &selectedActionIndex, availableActions, IM_ARRAYSIZE(availableActions));

        if (ImGui::Button("Attach Action")) {
            switch (selectedActionIndex) {
            case 0: AddAction<IdleMovementAction>(); break;
            case 1: AddAction<WalkMovementAction>(); break;
            case 2: AddAction<JumpMovementAction>(8.0f); break; // 初期ジャンプ力 8.0f
            case 3: AddAction<AirMovementAction>(); break;
            case 4: AddAction<SplineFollowMovementAction>(); break;
            }
        }

        ImGui::TreePop();
    }
#endif
}