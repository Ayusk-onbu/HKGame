#include "DynamicObject.h"

void DynamicObject::Initialize(Fngine* engine, std::string modelName, std::string textureName) {
	transform_.Initialize();
	transform_.LocalToWorld();
}

void DynamicObject::Update(float deltaTime) {
	for (auto& comp : components_) {
		comp->Update(deltaTime);
	}
	DrawUI();
}

void DynamicObject::Draw() {

}

void DynamicObject::OnCollision(DynamicObject* other, const Vector3& pushOut) {
	// 自分が持っているすべてのコンポーネントに「当たったぞ！」と通知する
	for (auto& comp : components_) {
		comp->OnCollision(other, pushOut);
	}
}

void DynamicObject::DrawUI() {
#ifdef USE_IMGUI
    if (ImGui::TreeNode(name_.empty() ? "DynamicObject" : name_.c_str())) {

        // --------------------------------------------------
        // 1. Transform の編集 UI
        // --------------------------------------------------
        if (ImGui::TreeNode("Transform")) {
            Vector3 pos = transform_.get_.Translation();
            Vector3 rot = transform_.get_.Rotation();
            Vector3 scale = transform_.get_.Scale();

            bool isEdited = false;
            if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) isEdited = true;
            if (ImGui::DragFloat3("Rotation", &rot.x, 0.5f)) isEdited = true;
            if (ImGui::DragFloat3("Scale", &scale.x, 0.05f)) isEdited = true;

            if (isEdited) {
                transform_.set_.Translation(pos);
                transform_.set_.Rotation(rot);
                transform_.set_.Scale(scale);
                transform_.LocalToWorld();
            }
            ImGui::TreePop();
        }

        ImGui::Separator();

        // --------------------------------------------------
        // 2. アタッチされている全コンポーネントの DrawUI 呼び出し ＆ 削除
        // --------------------------------------------------
        ImGui::Text("Components (%zu):", components_.size());

        for (size_t i = 0; i < components_.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));

            // 各コンポーネント固有の ImGui UI を描画
            components_[i]->DrawUI();

            // コンポーネント削除ボタン
            ImGui::SameLine();
            if (ImGui::Button("Remove Component")) {
                components_.erase(components_.begin() + i);
                ImGui::PopID();
                break;
            }

            ImGui::PopID();
        }

        ImGui::Separator();

        // --------------------------------------------------
        // 3. ComponentFactory と連携したコンポーネント追加 UI
        // --------------------------------------------------
        const char* availableComponents[] = {
            "Render",
            "ControllerComponent",
            "MovementComponent",
            "SkinningComponent",
            "RigidBody",
            "ColliderComponent",
            "StatusComponent"
        };
        static int selectedCompIdx = 0;

        ImGui::Combo("Add Component", &selectedCompIdx, availableComponents, IM_ARRAYSIZE(availableComponents));
        ImGui::SameLine();

        if (ImGui::Button("Attach")) {
            std::string compName = availableComponents[selectedCompIdx];

            // ComponentFactory 経由で動的にインスタンスを生成してアタッチ！
            auto newComp = ComponentFactory::GetInstance()->Create(compName);
            if (newComp) {
                newComp->master_ = this;
                components_.push_back(std::move(newComp));
            }
        }

        ImGui::TreePop();
    }
#endif
}

json DynamicObject::Serialize() const {
    json j;

    // オブジェクト基本情報（名前、Transformなど）
    j["name"] = name_;
    j["transform"] = {
        {"position", {transform_.transform_.translation_.x, transform_.transform_.translation_.y, transform_.transform_.translation_.z}},
        {"rotation", {transform_.transform_.rotation_.x, transform_.transform_.rotation_.y, transform_.transform_.rotation_.z}},
        {"scale",    {transform_.transform_.scale_.x, transform_.transform_.scale_.y, transform_.transform_.scale_.z}}
    };

    // アタッチされている全コンポーネントの書き出し
    json componentsJson = json::array();
    for (const auto& comp : components_) {
        json compJson;
        compJson["type"] = comp->GetComponentName(); // コンポーネントの種類
        compJson["data"] = comp->Serialize();        // コンポーネントの中身
        componentsJson.push_back(compJson);
    }
    j["components"] = componentsJson;

    return j;
}

void DynamicObject::Deserialize(const json& j) {
    // オブジェクトの情報を復元
    if (j.contains("name")) {
        name_ = j["name"].get<std::string>();
    }
    transform_.Initialize();
    if (j.contains("transform")) {
        const auto& transformJson = j["transform"];
        transform_.set_.Translation({
            transformJson["position"][0].get<float>(),
            transformJson["position"][1].get<float>(),
            transformJson["position"][2].get<float>()
        });
        transform_.set_.Rotation({
            transformJson["rotation"][0].get<float>(),
            transformJson["rotation"][1].get<float>(),
            transformJson["rotation"][2].get<float>()
        });
        transform_.set_.Scale({
            transformJson["scale"][0].get<float>(),
            transformJson["scale"][1].get<float>(),
            transformJson["scale"][2].get<float>()
        });
        transform_.LocalToWorld();
    }

    // コンポーネント群の再構築・復元
    if (j.contains("components") && j["components"].is_array()) {
        components_.clear(); // 既存のコンポーネントをクリアして再ロード

        for (const auto& compJson : j["components"]) {
            if (!compJson.contains("type")) continue;
            std::string type = compJson["type"].get<std::string>();

            // Factory 経由で生成
            auto newComp = ComponentFactory::GetInstance()->Create(type);
            if (newComp) {
                newComp->master_ = this;
                if (compJson.contains("data")) {
                    newComp->Deserialize(compJson["data"]);
                }
                components_.push_back(std::move(newComp));
            }
        }
    }
}