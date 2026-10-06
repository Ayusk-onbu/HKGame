#include "Controller.h"
#include "ImGuiManager.h"

json ControllerComponent::Serialize() const {
    json j;
    j["controllerName"] = controllerName_;
    return j;
}

void ControllerComponent::Deserialize(const json& j) {
    if (j.contains("controllerName")) {
        controllerName_ = j["controllerName"].get<std::string>();
        if (controllerName_ == "PlayerController") {
            SetController(std::make_unique<PlayerController>(), controllerName_);
        }
        else if (controllerName_ == "HermiteSplineController") {
            SetController(std::make_unique<HermiteSplineController>(), controllerName_);
        }
    }
}

void ControllerComponent::DrawUI() {
#ifdef USE_IMGUI
    if (ImGui::TreeNode("Controller Component")) {
        ImGui::Text("Active Controller: %s", controllerName_.c_str());
        ImGui::Separator();

        // ---------------------------------------------------
        // 1. Controller の選択とセット
        // ---------------------------------------------------
        const char* controllerTypes[] = {
            "PlayerController (Human Input)",
            "HermiteSplineController (Rail Patrol)"
        };
        static int selectedType = 0;

        ImGui::Combo("Select Controller", &selectedType, controllerTypes, IM_ARRAYSIZE(controllerTypes));
        ImGui::SameLine();

        if (ImGui::Button("Set Controller")) {
            switch (selectedType) {
            case 0:
                SetController(std::make_unique<PlayerController>(), "PlayerController");
                break;
            case 1:
                SetController(std::make_unique<HermiteSplineController>(), "HermiteSplineController");
                break;
            }
        }

        // ---------------------------------------------------
        // 2. HermiteSplineController 固有のパラメータ調整
        // ---------------------------------------------------
        if (auto* splineCtrl = dynamic_cast<HermiteSplineController*>(controller_.get())) {
            ImGui::Separator();
            ImGui::Text("Spline Controller Settings");

            static float speed = 0.1f;
            if (ImGui::DragFloat("Speed", &speed, 0.01f, 0.0f, 2.0f, "%.2f")) {
                splineCtrl->SetSpeed(speed);
            }

            static bool loop = true;
            if (ImGui::Checkbox("Loop Patrol", &loop)) {
                splineCtrl->SetLoop(loop);
            }

            ImGui::Text("Progress: %.2f", splineCtrl->GetProgress());
            if (ImGui::Button("Reset Progress")) {
                splineCtrl->ResetProgress();
            }
        }

        ImGui::TreePop();
    }
#endif
}