#pragma once
#include "Component.h"
#include "SkinningManager.h"
#include "AnimationManager.h"

class Fngine;

class SkinningComponent :
	public Component
{
public:
	SkinningComponent() = default;
	~SkinningComponent()override = default;

	void Initialize()override {}
	void Update(float deltaTime)override;
	void DrawUI()override;
    std::string GetComponentName() const override { return "SkinningComponent"; }
    json Serialize() const override;
    void Deserialize(const json& j) override;

    // --- セットアップ ---
    /// <summary>
    /// モデルIDを指定して Skeleton / SkinCluster の個別インスタンスを生成・初期化する
    /// </summary>
    /// <param name="engine">エンジンポインタ</param>
    /// <param name="modelID">ModelManager / SkinningManager に登録されているモデル識別名</param>
    bool Setup(Fngine* engine, const std::string& modelID);

    // --- GPU スキニング (Compute Shader) の実行 ---
    void DispatchCS(ID3D12GraphicsCommandList* commandList);

    // --- アニメーション再生制御 ---
    void PlayAnimation(const std::string& animName, bool loop = true, float speed = 1.0f);
    void PauseAnimation() { isPlaying_ = false; }
    void ResumeAnimation() { isPlaying_ = true; }
    void StopAnimation();

    void SetPlaybackSpeed(float speed) { playbackSpeed_ = speed; }
    void SetAnimationTime(float time) { animationTime_ = time; }
    void SetLoop(bool loop) { isLoop_ = loop; }

    // --- ゲッター ---
    Skeleton& GetSkeleton() { return skeleton_; }
    const Skeleton& GetSkeleton() const { return skeleton_; }

    SkinCluster& GetSkinCluster() { return skinCluster_; }
    const SkinCluster& GetSkinCluster() const { return skinCluster_; }

    // 描画時に必要な変形後の頂点バッファビュー(VBV)を取得
    D3D12_VERTEX_BUFFER_VIEW GetOutputVertexBufferView() const {
        return skinCluster_.outputVertices_ ? skinCluster_.GetOutputVertexBufferView() : D3D12_VERTEX_BUFFER_VIEW{};
    }

    const std::string& GetCurrentAnimationName() const { return currentAnimationName_; }
    bool IsPlaying() const { return isPlaying_; }
    float GetAnimationTime() const { return animationTime_; }
	std::string GetModelID() const { return modelID_; }

    // --- ボーン情報取得用インターフェース ---

    /// <summary>
    /// ボーンのモデルローカル行列（skeletonSpaceMatrix）を取得する
    /// </summary>
    std::optional<Matrix4x4> GetJointLocalMatrix(const std::string& jointName) const {
        return skeleton_.GetJointMatrix(jointName);
    }

    /// <summary>
    /// ボーンの「ワールド行列」を取得する（武器のアタッチやカメラ追従に最適）
    /// </summary>
    std::optional<Matrix4x4> GetJointWorldMatrix(const std::string& jointName) const;

    /// <summary>
    /// ボーンの「ワールド座標」を取得する（頭や足元の絶対位置を取得）
    /// </summary>
    std::optional<Vector3> GetJointWorldPosition(const std::string& jointName) const;

    /// <summary>
    /// インデックス検索用（ボーン名をあらかじめ保持して毎フレーム呼び出す場合）
    /// </summary>
    int32_t GetJointIndex(const std::string& jointName) const {
        return skeleton_.GetJointIndex(jointName);
    }

private:
    Fngine* engine_ = nullptr;
    std::string modelID_ = "";

    // このオブジェクトが個別所有する動的データ
    Skeleton skeleton_;
    SkinCluster skinCluster_;

    // アニメーション再生状態（インスタンスごとに管理）
    std::string currentAnimationName_ = "";
    float animationTime_ = 0.0f;
    float playbackSpeed_ = 1.0f;
    bool isLoop_ = true;
    bool isPlaying_ = false;
};