#include "RayTracingComponent.h"
#include "ModelManager.h"
#include "DynamicObject.h"
#include "../Skinning/SkinningComponent.h"
#include "StringPool.h"

void RayTracingComponent::Update(float deltaTime) {
    if (!master_) return;

    const auto& worldMat = master_->GetTransform().mat_;

    // スキニングコンポーネントがあれば動的スキニングメッシュとして登録
    if (auto* skinning = master_->GetComponent<SkinningComponent>()) {
        auto vbv = skinning->GetOutputVertexBufferView();
        auto& objData = ModelManager::GetInstance()->LoadObjectData(skinning->GetModelID());

        RayTracingManager::GetInstance()->RegisterSkinnedMeshInstance(
            vbv.BufferLocation,
            objData.GetIndexBufferView().BufferLocation,
            objData.GetVertexCount(),
            objData.GetIndexCount(),
            worldMat,
            color_,
			skinning->GetSkinCluster().GetOutputVertexBufferResource(),
			objData.GetIndexResource().Get()
        );
    }
    // なければ静的メッシュとして登録
    else {
        RayTracingManager::GetInstance()->RegisterStaticMeshInstance(
            modelID_,
            worldMat,
            color_
        );
    }
}

void RayTracingComponent::DrawUI() {
#ifdef USE_IMGUI
    ImGui::Text("RayTracing Component");
    ImGui::ColorEdit4("RT Color", &color_.x);

    std::string currentModelName = StringPool::GetInstance()->GetStringFromID(modelID_);
    auto modelNames = ModelManager::GetInstance()->GetBVHNames();

    if (ImGui::BeginCombo("RT Model", currentModelName.c_str())) {
        for (const auto& name : modelNames) {
            bool isSelected = (currentModelName == name);
            if (ImGui::Selectable(name.c_str(), isSelected)) {
                modelID_ = StringPool::GetInstance()->GetStringID(name);
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
#endif
}

json RayTracingComponent::Serialize() const {
    json j;
    j["modelName"] = StringPool::GetInstance()->GetStringFromID(modelID_);
    j["color"] = { color_.x, color_.y, color_.z, color_.w };
    return j;
}

void RayTracingComponent::Deserialize(const json& j) {
    if (j.contains("modelName")) {
        modelID_ = StringPool::GetInstance()->GetStringID(j["modelName"].get<std::string>());
    }
    if (j.contains("color") && j["color"].is_array()) {
        color_ = { j["color"][0], j["color"][1], j["color"][2], j["color"][3] };
    }
}