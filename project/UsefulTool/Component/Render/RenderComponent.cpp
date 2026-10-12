#include "RenderComponent.h"
#include "DynamicObject.h"
#include "SDFManager.h"
#include "StringPool.h"

void RenderComponent::Update(float deltaTime) {
    if (!master_ || !provider_) return;
    
    master_->GetTransform().LocalToWorld();
    provider_->SendToDrawManager(master_);

    if (castSDFShadow_) {
        // SDFに存在を追加する処理->影などの描画に繋がっている
        SDFManager::GetInstance()->AddInstance(
            modelID_,
            master_->GetTransform().mat_
        );
    }
}

json RenderComponent::Serialize() const {
    json j;
    if (dynamic_cast<NormalMeshRenderProvider*>(provider_.get())) {
        j["providerType"] = "NormalMesh";
    }
    else if (dynamic_cast<CharacterRenderProvider*>(provider_.get())) {
        j["providerType"] = "Character";
    }
    else if (dynamic_cast<GrassRenderProvider*>(provider_.get())) {
        j["providerType"] = "Grass";
    }
    else {
        j["providerType"] = "None";
    }

    if (provider_) {
        j["providerData"] = provider_->Serialize();
    }

    j["castSDFShadow"] = castSDFShadow_;

	j["ShadowModelName"] = StringPool::GetInstance()->GetStringFromID(modelID_);

    return j;
}

void RenderComponent::Deserialize(const json& j) {
    if (j.contains("providerType")) {
        std::string type = j["providerType"].get<std::string>();
        if (type == "Character") {
            SetProvider<CharacterRenderProvider>();
        }
        else if (type == "Grass") {
            SetProvider<GrassRenderProvider>();
        }
        else if (type == "NormalMesh") {
            SetProvider<NormalMeshRenderProvider>();
        }
    }

    if(j.contains("providerData")) {
        provider_->Deserialize(j["providerData"]);
    }
    if (j.contains("ShadowModelName")) {
        std::string modelName = j["ShadowModelName"].get<std::string>();
        modelID_ = StringPool::GetInstance()->GetStringID(modelName);
    }
}

void RenderComponent::DrawUI() {
#ifdef USE_IMGUI
    ImGui::Text("Render Provider Selection");

    // プロバイダー切り替え UI
    const char* providers[] = { "Character", "Grass", "NormalMesh" };
    int currentIdx = 0;
    if (dynamic_cast<CharacterRenderProvider*>(provider_.get())) currentIdx = 1;
    else if (dynamic_cast<GrassRenderProvider*>(provider_.get())) currentIdx = 2;

    if (ImGui::Combo("Provider Type", &currentIdx, providers, IM_ARRAYSIZE(providers))) {
        if (currentIdx == 0) SetProvider<CharacterRenderProvider>();
        else if (currentIdx == 1) SetProvider<GrassRenderProvider>();
        else if (currentIdx == 2) SetProvider<NormalMeshRenderProvider>();
    }

    ImGui::Separator();

    // Modelの選択
    std::string currentModelName = StringPool::GetInstance()->GetStringFromID(modelID_);
    auto modelNames = ModelManager::GetInstance()->GetBVHNames();

    if (ImGui::BeginCombo("Shadow Model", currentModelName.c_str())) {
        for (const auto& name : modelNames) {
            bool isSelected = (currentModelName == name);
            if (ImGui::Selectable(name.c_str(), isSelected)) {
                // 文字列から StringPool ID (uint32_t) へ即座に変換して保持
                modelID_ = StringPool::GetInstance()->GetStringID(name);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Text("Shadow Model ID: %u", modelID_);

    // 現在保持している Provider 固有の UI を描画
    if (provider_) {
        provider_->DrawUI();
    }
    else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "No RenderProvider Attached!");
        return;
    }
#endif
}