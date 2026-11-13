#include "SDFFunctionComponent.h"
#include "DynamicObject.h"
#include "SDFFunctionSystem.h"
#include "../Skinning/SkinningComponent.h"
#include "FileSystem.h"

void SDFFunctionComponent::Initialize(){ 
    if (nodes_.empty()) {
        SDFFunctionStackNodeCPU defaultSphere;
        defaultSphere.type = SDFFunctionStackNodeType::Sphere;
        defaultSphere.params = Vector3(1.0f, 0.0f, 0.0f); // 半径 1.0
        AddNode(defaultSphere);
    }
    OnEnable(); 
}

void SDFFunctionComponent::RecalculateAABB() {
    GetCurrentLocalAABB(localAABBMin_, localAABBMax_);
}

void SDFFunctionComponent::ResolveBoneBindings() {
    if (!master_) return;
    auto* skinComp = master_->GetComponent<SkinningComponent>();
    if (!skinComp) return;

    for (auto& node : nodes_) {
        if (!node.followJointName.empty() && node.pFollowMatrix == nullptr) {
            node.pFollowMatrix = skinComp->GetJointLocalMatrixPtr(node.followJointName);
        }
    }
}

void SDFFunctionComponent::DrawUI() {
#ifdef USE_IMGUI
    ImGui::Text("Node Count: %zu", nodes_.size());

    auto addShapeWithAutoUnion = [&](SDFFunctionStackNodeType shapeType, const Vector3& defaultParams) {
        bool hasExistingShape = !nodes_.empty();

        SDFFunctionStackNodeCPU shapeNode;
        shapeNode.type = shapeType;
        shapeNode.params = defaultParams;
        AddNode(shapeNode);

        if (hasExistingShape) {
            SDFFunctionStackNodeCPU opNode;
            opNode.type = SDFFunctionStackNodeType::OpUnion;
            opNode.smoothK = 0.2f;
            AddNode(opNode);
        }
        };

    if (ImGui::Button("Add Sphere")) {
        addShapeWithAutoUnion(SDFFunctionStackNodeType::Sphere, Vector3(0.2f, 0.0f, 0.0f));
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Box")) {
        addShapeWithAutoUnion(SDFFunctionStackNodeType::Box, Vector3(0.2f, 0.2f, 0.2f));
    }

    ImGui::Separator();

    // 同じオブジェクトに SkinningComponent があればボーン一覧を取得
    SkinningComponent* skinComp = master_ ? master_->GetComponent<SkinningComponent>() : nullptr;
    std::vector<std::string> jointNames;
    if (skinComp) {
        jointNames = skinComp->GetJointNames();
    }

    bool isEdited = false;
    static char boneFilter[64] = "";

    for (size_t i = 0; i < nodes_.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        auto& node = nodes_[i];

        if (static_cast<uint32_t>(node.type) < 100) {
            const char* shapeNames[] = { "Sphere", "Box", "Cylinder", "Torus" };
            int currentShape = static_cast<int>(node.type);
            if (ImGui::Combo("Shape", &currentShape, shapeNames, IM_ARRAYSIZE(shapeNames))) {
                node.type = static_cast<SDFFunctionStackNodeType>(currentShape);
                isEdited = true;
            }

            // --- 追従ボーン選択 UI ---
            if (skinComp && !jointNames.empty()) {
                const char* previewName = node.followJointName.empty() ? "None (Root)" : node.followJointName.c_str();
                if (ImGui::BeginCombo("Follow Bone", previewName)) {
                    // ボーン数が多い時用の簡易検索フィルタ
                    ImGui::InputText("Search", boneFilter, IM_ARRAYSIZE(boneFilter));
                    ImGui::Separator();

                    // 追従解除オプション
                    bool isNoneSelected = node.followJointName.empty();
                    if (ImGui::Selectable("None (Root)", isNoneSelected)) {
                        node.followJointName.clear();
                        node.pFollowMatrix = nullptr;
                        isEdited = true;
                    }

                    // 全ボーンリストの表示
                    for (const auto& jName : jointNames) {
                        if (boneFilter[0] != '\0' && jName.find(boneFilter) == std::string::npos) {
                            continue;
                        }
                        bool isSelected = (node.followJointName == jName);
                        if (ImGui::Selectable(jName.c_str(), isSelected)) {
                            node.followJointName = jName;
                            node.pFollowMatrix = skinComp->GetJointLocalMatrixPtr(jName);
                            // ボーンを選んだ直後は、ボーンの原点にピタッと合うようローカル座標をリセットすると調整しやすい
                            node.position = Vector3(0.0f, 0.0f, 0.0f);
                            isEdited = true;
                        }
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
            }

            if (ImGui::DragFloat3("Pos", &node.position.x, 0.02f)) isEdited = true;
            if (ImGui::DragFloat3("Rotation", &node.rotation.x, 0.05f)) isEdited = true;
            if (ImGui::DragFloat3("Scale", &node.scale.x, 0.05f)) isEdited = true;
            if (ImGui::DragFloat3("Params", &node.params.x, 0.02f)) isEdited = true;
        }
        else {
            const char* opNames[] = { "Union (+)", "Subtract (-)", "Intersection", "Smooth Union", "Smooth Subtract" };
            int currentOp = static_cast<int>(node.type) - 100;
            if (ImGui::Combo("Operation", &currentOp, opNames, IM_ARRAYSIZE(opNames))) {
                node.type = static_cast<SDFFunctionStackNodeType>(currentOp + 100);
            }

            if (node.type == SDFFunctionStackNodeType::OpSmoothUnion || node.type == SDFFunctionStackNodeType::OpSmoothSubtract) {
                ImGui::DragFloat("Smooth K", &node.smoothK, 0.01f, 0.001f, 5.0f);
            }
        }

        if (skinComp && !node.followJointName.empty()) {
            if (auto wPos = skinComp->GetJointWorldPosition(node.followJointName)) {
                ImGui::Text("Bone World Pos: (%.2f, %.2f, %.2f)", wPos->x, wPos->y, wPos->z);
            }
        }

        ImGui::Separator();
        ImGui::PopID();
    }

    if (isEdited) {
        RecalculateAABB();
    }

    if (ImGui::Button("Save")) {
        json splineJson = Serialize();
        FileSystem::SaveWithDialog(splineJson);
    }
#endif
}

void SDFFunctionComponent::GetCurrentLocalAABB(Vector3& outMin, Vector3& outMax) const {
    if (nodes_.empty()) {
        outMin = localAABBMin_;
        outMax = localAABBMax_;
        return;
    }

    Vector3 gMin(FLT_MAX, FLT_MAX, FLT_MAX);
    Vector3 gMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    bool hasShape = false;

    for (const auto& node : nodes_) {
        if (static_cast<uint32_t>(node.type) < 100) {
            hasShape = true;
            float maxParam = (std::max)({ node.params.x, node.params.y, node.params.z });
            if (node.type == SDFFunctionStackNodeType::Torus) {
                maxParam = node.params.x + node.params.y;
            }
            float maxScale = (std::max)({ std::abs(node.scale.x), std::abs(node.scale.y), std::abs(node.scale.z) });
            float boundRadius = maxParam * maxScale;

            Vector3 center = node.position;
            // ボーン追従中は、そのボーンの現在位置（平行移動成分）を中心に加算
            if (node.pFollowMatrix != nullptr) {
                center.x += node.pFollowMatrix->m[3][0];
                center.y += node.pFollowMatrix->m[3][1];
                center.z += node.pFollowMatrix->m[3][2];
            }

            Vector3 nMin = center - Vector3(boundRadius, boundRadius, boundRadius);
            Vector3 nMax = center + Vector3(boundRadius, boundRadius, boundRadius);

            gMin.x = (std::min)(gMin.x, nMin.x); gMin.y = (std::min)(gMin.y, nMin.y); gMin.z = (std::min)(gMin.z, nMin.z);
            gMax.x = (std::max)(gMax.x, nMax.x); gMax.y = (std::max)(gMax.y, nMax.y); gMax.z = (std::max)(gMax.z, nMax.z);
        }
    }

    if (hasShape) {
        outMin = gMin;
        outMax = gMax;
    }
    else {
        outMin = localAABBMin_;
        outMax = localAABBMax_;
    }
}

void SDFFunctionComponent::OnEnable() {
    SDFSystem::GetInstance()->Register(this);
}

void SDFFunctionComponent::OnDisable() {
    SDFSystem::GetInstance()->Unregister(this);
}

json SDFFunctionComponent::Serialize() const {
    json j;
    json nodesArr = json::array();
    for (const auto& node : nodes_) {
        json nJson;
        nJson["type"] = static_cast<uint32_t>(node.type);
        nJson["params"] = { node.params.x, node.params.y, node.params.z };
        nJson["pos"] = { node.position.x, node.position.y, node.position.z };
        nJson["rot"] = { node.rotation.x, node.rotation.y, node.rotation.z };
        nJson["scale"] = { node.scale.x, node.scale.y, node.scale.z };
        nJson["smoothK"] = node.smoothK;
        nJson["followJointName"] = node.followJointName;
        nodesArr.push_back(nJson);
    }
    j["nodes"] = nodesArr;
    return j;
}

void SDFFunctionComponent::Deserialize(const json& j) {
    nodes_.clear();
    if (j.contains("nodes") && j["nodes"].is_array()) {
        for (const auto& nJson : j["nodes"]) {
            SDFFunctionStackNodeCPU node;
            node.type = static_cast<SDFFunctionStackNodeType>(nJson["type"].get<uint32_t>());
            node.params = { nJson["params"][0], nJson["params"][1], nJson["params"][2] };
            node.position = { nJson["pos"][0], nJson["pos"][1], nJson["pos"][2] };
            node.rotation = { nJson["rot"][0], nJson["rot"][1], nJson["rot"][2] };
            node.scale = { nJson["scale"][0], nJson["scale"][1], nJson["scale"][2] };
            node.smoothK = nJson["smoothK"].get<float>();
            node.followJointName = nJson["followJointName"].get<std::string>();
            nodes_.push_back(node);
        }
    }
    ResolveBoneBindings();
    RecalculateAABB();
}

bool SDFFunctionComponent::LoadFromFile(const std::string& filePath) {
    if (filePath.empty()) {
        return false;
    }

    std::ifstream file(filePath);
    if (!file.is_open()) {
        Log::View("SDFFunctionComponent Error: Failed to open file: " + filePath);
        return false;
    }

    try {
        json j;
        file >> j;
        Deserialize(j);
    }
    catch (const std::exception& e) {
        Log::View(std::string("SDFFunctionComponent JSON Parse Error: ") + e.what());
        return false;
    }

    return true;
}