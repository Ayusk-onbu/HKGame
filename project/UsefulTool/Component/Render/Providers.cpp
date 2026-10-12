#include "Providers.h"
#include "DynamicObject.h"
#include "../Skinning/SkinningComponent.h"
#include "ModelManager.h"
#include "StringPool.h"

void GrassRenderProvider::SendToDrawManager(DynamicObject* owner){
    data.worldTransform = owner->GetTransform();
    DrawManager::GetInstance()->GetGrass()->AddInstance(data);
}

void GrassRenderProvider::DrawUI() {
#ifdef USE_IMGUI
    ImGui::Text("Provider: Grass Render");

    auto* texMgr = TextureManager::GetInstance();

    // 1. 現在設定されている Bindless ID (SRV Index) からテクスチャ名を取得
    // ※ data 側で Bindless ID を保持している変数名（例: data.textureIndex ）に合わせて変更してください
    std::string currentTexName = texMgr->GetTextureNameBySrvIndex(data.textureIndex);

    // 2. ImGui Combo (ドロップダウン) で TextureManager 内のテクスチャ一覧を表示
    if (ImGui::BeginCombo("Grass Texture", currentTexName.c_str())) {

        for (const auto& [name, tex] : texMgr->GetData()) {
            if (!tex) continue;

            bool isSelected = (currentTexName == name);

            // テクスチャ名とSRVインデックスを一覧表示
            std::string label = name + " (ID: " + std::to_string(tex->GetSrvIndex()) + ")";

            if (ImGui::Selectable(label.c_str(), isSelected)) {
                // 選択したテクスチャの Bindless ID (SRV Index) を GrassObjectData にセット！
                data.textureIndex = tex->GetSrvIndex();
            }

            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // 現在適用されている Bindless ID を数値でデバッグ表示
    ImGui::Text("Applied SRV Index: %u", data.textureIndex);

    // GrassObjectData 内にカラーやパラメータがあればここで編集
    // 例: カラー編集（Vector4 や Float4 の場合）
    ImGui::ColorEdit4("Grass Color", &data.color.x);

    // 追加のパラメータ例（草の揺れ速度、サイズ、テクスチャIDなど）
    ImGui::DragFloat("Wind Scale", &data.windPhase, 0.01f);
#endif
}

void CharacterRenderProvider::SendToDrawManager(DynamicObject* owner) {
    if (!owner) return;

    // ワールドトランスフォームを設定
    data.worldTransform = owner->GetTransform();

	data.melaninScale = data.melaninScale; // ここで必要に応じてスケールを設定
	data.bloodVolumeScale = data.bloodVolumeScale;
	data.roughnessScale = data.roughnessScale;
	data.sssScale = data.sssScale;

    // SkinningComponent が存在するかチェック
    if (auto* skinning = owner->GetComponent<SkinningComponent>()) {
        data.modelName = skinning->GetModelID();
        // GPU スキニング後の最新 VBV をセット！
        data.vertexBufferView = skinning->GetOutputVertexBufferView();
        data.useCustomVB = true;
    }
    else {
        // スキニングがない場合は静的メッシュの VBV をセット（またはデフォルトフラグ）
        data.useCustomVB = false;
    }

    // DrawManager 経由で PrimitiveCharacter に登録！
    DrawManager::GetInstance()->GetCharacter()->AddInstance(data);
}

void CharacterRenderProvider::DrawUI() {
#ifdef USE_IMGUI
    if (ImGui::TreeNode("Character Render Settings")) {
        ImGui::Text("Skinning Mode: %s", data.useCustomVB ? "Active (GPU Skinned)" : "Static Mesh");

        // カラーピッカー
        ImGui::ColorEdit4("Color", &data.color.x);

		ImGui::DragFloat("Melanin Scale", &data.melaninScale, 0.01f, 0.0f, 2.0f);
		ImGui::DragFloat("Blood Volume Scale", &data.bloodVolumeScale, 0.01f, 0.0f, 2.0f);
		ImGui::DragFloat("Roughness Scale", &data.roughnessScale, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("SSS Scale", &data.sssScale, 0.01f, 0.0f, 2.0f);

        ImGui::TreePop();
    }
#endif
}

void NormalMeshRenderProvider::SendToDrawManager(DynamicObject* owner) {
    if (!owner) return;

    // ワールドトランスフォームを設定
    data.worldTransform = owner->GetTransform();

    // DrawManager の NormalMesh 描画パスにインスタンス登録！
    DrawManager::GetInstance()->GetNormalMesh()->AddInstance(data);
}

void NormalMeshRenderProvider::DrawUI() {
#ifdef USE_IMGUI
    ImGui::Text("Provider: Normal Mesh Render");

    // --- モデル選択 (ModelManager / StringPool 連携) ---
    std::string currentModelName = StringPool::GetInstance()->GetStringFromID(data.modelID);
    auto modelNames = ModelManager::GetInstance()->GetBVHNames();

    if (ImGui::BeginCombo("Model", currentModelName.c_str())) {
        for (const auto& name : modelNames) {
            bool isSelected = (currentModelName == name);
            if (ImGui::Selectable(name.c_str(), isSelected)) {
                // 文字列から StringPool ID (uint32_t) へ即座に変換して保持
                data.modelID = StringPool::GetInstance()->GetStringID(name);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Text("Model ID: %u", data.modelID);

    // --- テクスチャ選択 (SRV Index 連携) ---
    auto* texMgr = TextureManager::GetInstance();
    std::string currentTexName = texMgr->GetTextureNameBySrvIndex(data.textureIndex);

    if (ImGui::BeginCombo("Texture", currentTexName.c_str())) {
        for (const auto& [name, tex] : texMgr->GetData()) {
            if (!tex) continue;
            bool isSelected = (currentTexName == name);
            std::string label = name + " (ID: " + std::to_string(tex->GetSrvIndex()) + ")";

            if (ImGui::Selectable(label.c_str(), isSelected)) {
                data.textureIndex = tex->GetSrvIndex();
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Text("Applied SRV Index: %u", data.textureIndex);

    // --- カラーピッカー ---
    ImGui::ColorEdit4("Color", &data.color.x);
#endif
}

json NormalMeshRenderProvider::Serialize() const {
    json j;
    // 数値IDではなく文字列名で保存（ロード環境が変わっても安全にするため）
    j["modelName"] = StringPool::GetInstance()->GetStringFromID(data.modelID);
    j["textureIndex"] = data.textureIndex;
    j["color"] = { data.color.x, data.color.y, data.color.z, data.color.w };
    return j;
}

void NormalMeshRenderProvider::Deserialize(const json& j) {
    if (j.contains("modelName")) {
        std::string modelName = j["modelName"].get<std::string>();
        data.modelID = StringPool::GetInstance()->GetStringID(modelName);
    }
    if (j.contains("textureIndex")) {
        data.textureIndex = j["textureIndex"].get<uint32_t>();
    }
    if (j.contains("color") && j["color"].is_array()) {
        data.color.x = j["color"][0].get<float>();
        data.color.y = j["color"][1].get<float>();
        data.color.z = j["color"][2].get<float>();
        data.color.w = j["color"][3].get<float>();
    }
}