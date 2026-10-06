#include "ColliderComponent.h"
#include "DynamicObject.h"
#include "CollisionManager.h"
#include "CollisionLayerManager.h"
#include "ModelManager.h"
#include "RigidBody.h"
#include "../Status/Status.h"
#include "../Status/DamageSource.h"

void ColliderComponent::DrawUI() {
#ifdef USE_IMGUI
    auto* layerMgr = CollisionLayerManager::GetInstance();

    // ---------------------------------------------------
    // 1. レイヤー設定 (MyType & Mask)
    // ---------------------------------------------------
    if (ImGui::TreeNode("Collision Layers")) {
        const auto& layers = layerMgr->GetLayers();

        // --- 自レイヤー (My Layer) ---
        uint32_t currentBit = collider_ ? collider_->GetMyType() : attribute_;
        std::string currentLayerName = layerMgr->GetNameByBit(currentBit);

        if (ImGui::BeginCombo("My Layer", currentLayerName.c_str())) {
            for (const auto& name : layers) {
                bool isSelected = (currentLayerName == name);
                if (ImGui::Selectable(name.c_str(), isSelected)) {
                    SetLayer(name); // ★ SetLayer 内で attribute_ も更新される
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // --- 衝突対象マスク (Target Mask) ---
        if (ImGui::TreeNode("Target Mask (Collides With)")) {
            uint32_t currentMask = collider_ ? collider_->GetYourType() : mask_;

            for (const auto& name : layers) {
                uint32_t bit = layerMgr->GetBitByName(name);
                bool isTarget = (currentMask & bit) != 0;

                if (ImGui::Checkbox(name.c_str(), &isTarget)) {
                    if (isTarget) {
                        currentMask |= bit;
                    }
                    else {
                        currentMask &= ~bit;
                    }
                    // ★ mask_ と collider_ の両方に確実に反映
                    mask_ = currentMask;
                    if (collider_) {
                        collider_->SetYourType(mask_);
                    }
                }
            }
            ImGui::TreePop();
        }

        // --- 新規レイヤーの追加 ---
        static char newLayerBuffer[64] = "";
        ImGui::InputText("New Layer Name", newLayerBuffer, sizeof(newLayerBuffer));
        ImGui::SameLine();
        if (ImGui::Button("Add Layer")) {
            if (strlen(newLayerBuffer) > 0) {
                if (layerMgr->AddLayer(newLayerBuffer)) {
                    newLayerBuffer[0] = '\0';
                }
            }
        }

        ImGui::TreePop();
    }

    ImGui::Separator();

    // ---------------------------------------------------
    // 2. コライダーの形状切り替え・生成
    // ---------------------------------------------------
    ImGui::Text("Collider Type");

    if (ImGui::Button("Set MeshCollider")) {
        CreateCollider<MeshCollider>();
    }
    ImGui::SameLine();
    if (ImGui::Button("Set BVHCollider")) {
        CreateCollider<BVHCollider>();
    }

    if (!collider_) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "No Collider Attached!");
        return;
    }

    ImGui::Separator();

    // ---------------------------------------------------
    // 3. 各コライダーのパラメーター設定
    // ---------------------------------------------------
    if (auto* meshCol = dynamic_cast<MeshCollider*>(collider_.get())) {
        ImGui::Text("Shape: Mesh Collider (Convex)");
        ImGui::Text("Vertices Count: %zu", meshCol->GetVertices().size());

        if (ImGui::Button("Clear Vertices")) {
            meshCol->ClearVertices();
        }
    }
    else if (auto* bvhCol = dynamic_cast<BVHCollider*>(collider_.get())) {
        ImGui::Text("Shape: BVH Collider (Static / Complex Map)");

        auto bvhNames = ModelManager::GetInstance()->GetBVHNames();
        std::string currentModel = bvhCol->GetModelName();
        if (currentModel.empty()) currentModel = "None (Select BVH)";

        if (ImGui::BeginCombo("BVH Model", currentModel.c_str())) {
            for (const auto& name : bvhNames) {
                bool isSelected = (currentModel == name);
                if (ImGui::Selectable(name.c_str(), isSelected)) {
                    bvhCol->SetModelName(name);
                    bvhCol->UpdateAABB();
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (bvhCol->GetBVH() != nullptr) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Status: BVH Loaded Successfully");
        }
        else {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Status: BVH Not Found or Not Selected");
        }
    }
#endif
}

void ColliderComponent::Initialize() {
    SetupCollider();
}

json ColliderComponent::Serialize() const {
    json j;
    j["attribute"] = attribute_;
    j["mask"] = mask_;

    if (dynamic_cast<MeshCollider*>(collider_.get())) {
        j["colliderType"] = "Mesh";
    }
    else if (auto* bvhCol = dynamic_cast<BVHCollider*>(collider_.get())) {
        j["colliderType"] = "BVH";
        j["bvhModelName"] = bvhCol->GetModelName();
    }
    else {
        j["colliderType"] = "None";
    }
    return j;
}

void ColliderComponent::Deserialize(const json& j) {
    if (j.contains("attribute")) attribute_ = j["attribute"].get<uint32_t>();
    if (j.contains("mask")) mask_ = j["mask"].get<uint32_t>();

    if (j.contains("colliderType")) {
        std::string type = j["colliderType"].get<std::string>();
        if (type == "Mesh") {
            CreateCollider<MeshCollider>();
        }
        else if (type == "BVH") {
            auto* bvhCol = CreateCollider<BVHCollider>();
            if (j.contains("bvhModelName") && bvhCol) {
                bvhCol->SetModelName(j["bvhModelName"].get<std::string>());
                bvhCol->UpdateAABB();
            }
        }
    }
    SetupCollider();
}

void ColliderComponent::SetupCollider() {
    if (!collider_) return;

    // 1. 持ち主のポインタを登録
    if (master_) {
        collider_->SetUserData(master_);
    }

    // 2. レイヤー属性・マスクの適用
    collider_->SetMyType(attribute_);
    collider_->SetYourType(mask_);

    // 3. 衝突コールバックの設定（親オブジェクトへの通知 & 物理押し戻し）
    collider_->onCollisionCallBack = [this](Collider* other, const Vector3& pushOut) {
        if (!master_ || !other) return;

        // 同じオブジェクトについている RigidBody を取得して、めり込み補正・着地・速度減衰を適用
        if (auto* rb = master_->GetComponent<RigidBody>()) {
            rb->ResolveCollision(pushOut);
        }

        // 1. 相手の GameObject を取得
        auto* targetObj = static_cast<DynamicObject*>(other->GetUserData());
        if (!targetObj) return;

        // 2. 相手が HP（StatusComponent）を持っているか？
        auto* targetStatus = targetObj->GetComponent<StatusComponent>();
        if (!targetStatus) return;

        // 3. 自身に DamageSourceComponent が付いているか調べる
        auto* dmgSource = master_->GetComponent<DamageSourceComponent>();
        if (targetStatus && dmgSource) {
            // 攻撃力を動的に計算（持ち主・武器・モショーン倍率などが合算される）
            float rawDamage = dmgSource->CalculateRawDamage(targetStatus);
            Element element = dmgSource->GetElement();

            // 相手にダメージを与える
            targetStatus->TakeDamage(rawDamage, element);

            // 状態異常バフ・デバフの付与
            for (const auto& effect : dmgSource->GetStatusEffects()) {
                targetStatus->AddEffect(effect);
            }
        }
        master_->OnCollision(targetObj, pushOut);
    };
}

void ColliderComponent::Update(float deltaTime) {
    if (!master_ || !collider_) return;

    // Transformの同期
    if (auto* meshCol = dynamic_cast<MeshCollider*>(collider_.get())) {
        meshCol->SetWorldMatrix(master_->GetTransform().mat_);
        meshCol->Update();
    }
    else if (auto* bvhCol = dynamic_cast<BVHCollider*>(collider_.get())) {
        bvhCol->SetWorldMatrix(master_->GetTransform().mat_);
    }
    else {
        collider_->SetWorldPosition(master_->GetTransform().get_.Translation());
    }

    // このフレームの判定用リストへ自動登録！
    CollisionManager::GetInstance()->SetColliders(collider_.get());
}

void ColliderComponent::SetLayer(const std::string& layerName) {
    attribute_ = CollisionLayerManager::GetInstance()->GetBitByName(layerName);
    if (collider_) {
        collider_->SetMyType(attribute_);
    }
}

void ColliderComponent::SetTargetLayers(const std::vector<std::string>& targetLayerNames) {
    uint32_t mask = 0;
    for (const auto& name : targetLayerNames) {
        mask |= CollisionLayerManager::GetInstance()->GetBitByName(name);
    }
    // 内部変数に保持
    mask_ = mask;
    if (collider_) {
        collider_->SetYourType(mask_);
    }
}