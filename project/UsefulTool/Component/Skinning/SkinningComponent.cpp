#include "SkinningComponent.h"
#include "DynamicObject.h"
#include "ModelManager.h"
#include "DrawManager.h"
#include "Log.h"

bool SkinningComponent::Setup(Fngine* engine, const std::string& modelID) {
    engine_ = engine;
    modelID_ = modelID;

    // 1. SkinningManager の静的テンプレートから Skeleton インスタンスを複製生成
    const SkeletonTemplate* skelTemplate = SkinningManager::GetInstance()->GetSkeletonTemplate(modelID_);
    if (!skelTemplate) {
        Log::View("SkinningComponent Error: SkeletonTemplate not found for ID: " + modelID_);
        return false;
    }
    skeleton_.CreateFromTemplate(*skelTemplate);

    // 2. ModelManager の ObjectData と静的データから SkinCluster インスタンスを初期化
    ObjectData& objData = ModelManager::GetInstance()->LoadObjectData(modelID_);
    skinCluster_.Create(engine_, modelID_, objData);

    return true;
}

void SkinningComponent::Update(float deltaTime) {
    if (!engine_ || modelID_.empty()) return;

    // 1. アニメーション時間を進めて Skeleton へポーズを適用
    if (isPlaying_ && !currentAnimationName_.empty()) {
        const AnimationClip* clip = AnimationManager::GetInstance()->GetAnimation(currentAnimationName_);
        if (clip) {
            animationTime_ += deltaTime * playbackSpeed_;

            // ループ & 終端処理
            if (clip->duration_ > 0.0f) {
                if (isLoop_) {
                    animationTime_ = std::fmod(animationTime_, clip->duration_);
                    if (animationTime_ < 0.0f) animationTime_ += clip->duration_;
                }
                else {
                    if (animationTime_ >= clip->duration_) {
                        animationTime_ = clip->duration_;
                        isPlaying_ = false; // 再生完了
                    }
                }
            }

            // 指定時刻の姿勢を Skeleton に適用！
            clip->ApplyToSkeleton(skeleton_, animationTime_);
        }
    }

    // 骨格の階層行列（skeletonSpaceMatrix）を計算・更新
    skeleton_.Update();

    // GPU のパレットバッファ(WellForGPU)へ書き込み
    skinCluster_.Update(skeleton_);

    SkinningManager::GetInstance()->RegisterActiveSkinCluster(&skinCluster_);

    //StopAnimation();
}

json SkinningComponent::Serialize() const {
    json j;
    j["modelID"] = modelID_;
    j["currentAnimationName"] = currentAnimationName_;
    j["playbackSpeed"] = playbackSpeed_;
    j["isLoop"] = isLoop_;
    return j;
}

void SkinningComponent::Deserialize(const json& j) {
    if (j.contains("modelID")) modelID_ = j["modelID"].get<std::string>();
    if (j.contains("currentAnimationName")) currentAnimationName_ = j["currentAnimationName"].get<std::string>();
    if (j.contains("playbackSpeed")) playbackSpeed_ = j["playbackSpeed"].get<float>();
    if (j.contains("isLoop")) isLoop_ = j["isLoop"].get<bool>();
}

void SkinningComponent::DispatchCS(ID3D12GraphicsCommandList* commandList) {
    if (!commandList) return;
    // GPU スキニング Compute Shader を発行して outputVertices_ を更新
    skinCluster_.DispatchComputeShader(commandList);
}

void SkinningComponent::PlayAnimation(const std::string& animName, bool loop, float speed) {
    if (!AnimationManager::GetInstance()->HasAnimation(animName)) {
        Log::View("SkinningComponent Warning: Animation not found: " + animName);
        return;
    }

    // 同じアニメーションの再再生で時間をリセット
    if (currentAnimationName_ != animName) {
        currentAnimationName_ = animName;
        animationTime_ = 0.0f;
    }

    isLoop_ = loop;
    playbackSpeed_ = speed;
    isPlaying_ = true;
}

void SkinningComponent::StopAnimation() {
    isPlaying_ = false;
    animationTime_ = 0.0f;
}

void SkinningComponent::DrawUI() {
#ifdef USE_IMGUI
    if (ImGui::TreeNode("Skinning Component")) {
        ImGui::Text("Model ID: %s", modelID_.empty() ? "Unassigned" : modelID_.c_str());
        ImGui::Text("Joints Count: %zu", skeleton_.joints_.size());
        ImGui::Text("Vertices Count: %u", skinCluster_.GetNumVertices());

        ImGui::Separator();

        // --- アニメーション選択ドロップダウン ---
        auto animNames = AnimationManager::GetInstance()->GetAnimationNames();
        if (!animNames.empty()) {
            std::string comboLabel = currentAnimationName_.empty() ? "Select Animation..." : currentAnimationName_;
            if (ImGui::BeginCombo("Animation", comboLabel.c_str())) {
                for (const auto& name : animNames) {
                    bool isSelected = (currentAnimationName_ == name);
                    if (ImGui::Selectable(name.c_str(), isSelected)) {
                        PlayAnimation(name, isLoop_, playbackSpeed_);
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        else {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "No Animations Loaded in AnimationManager");
        }

        // --- 再生状態コントローラー ---
        if (!currentAnimationName_.empty()) {
            const AnimationClip* clip = AnimationManager::GetInstance()->GetAnimation(currentAnimationName_);
            float maxDuration = clip ? clip->duration_ : 1.0f;

            if (isPlaying_) {
                if (ImGui::Button("Pause")) PauseAnimation();
            }
            else {
                if (ImGui::Button("Play")) ResumeAnimation();
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop")) StopAnimation();

            ImGui::SliderFloat("Time", &animationTime_, 0.0f, maxDuration, "%.2f sec");
            ImGui::DragFloat("Speed", &playbackSpeed_, 0.05f, -5.0f, 5.0f, "%.2f");
            ImGui::Checkbox("Loop", &isLoop_);
        }

        ImGui::TreePop();
    }

    //DrawManager::GetInstance()->GetBone()->AddSkeleton(skeleton_, master_->GetTransform().mat_);
#endif
}

std::optional<Matrix4x4> SkinningComponent::GetJointWorldMatrix(const std::string& jointName) const {
    auto localMatOpt = skeleton_.GetJointMatrix(jointName);
    if (!localMatOpt || !master_) return std::nullopt;

    // キャラクター本体のワールド行列 × ボーンのローカル行列
    Matrix4x4 characterWorldMat = master_->GetTransform().mat_;
    return (*localMatOpt) * characterWorldMat;
}

std::optional<Vector3> SkinningComponent::GetJointWorldPosition(const std::string& jointName) const {
    auto worldMatOpt = GetJointWorldMatrix(jointName);
    if (!worldMatOpt) return std::nullopt;

    // 行列の平行移動成分(4列目: m[3][0], m[3][1], m[3][2])を抜く
    return Vector3(
        worldMatOpt->m[3][0],
        worldMatOpt->m[3][1],
        worldMatOpt->m[3][2]
    );
}