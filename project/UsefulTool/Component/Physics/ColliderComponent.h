#pragma once
#include "Component.h"
#include "Collider.h"

class ColliderComponent : public Component {
public:
    ColliderComponent() = default;
    virtual ~ColliderComponent() = default;

    // --- コンポーネントの基本処理 ---
    void Initialize() override;
    void Update(float deltaTime) override;
    void DrawUI()override;
    std::string GetComponentName() const override { return "ColliderComponent"; }
    json Serialize() const override;
    void Deserialize(const json& j) override;

    // --- コライダーの生成・セット ---
    template <typename T, typename... Args>
    T* CreateCollider(Args&&... args) {
        auto collider = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = collider.get();
        collider_ = std::move(collider);
        SetupCollider();
        return ptr;
    }

    Collider* GetCollider() const { return collider_.get(); }

    // レイヤー名（文字列）で衝突属性・マスクを設定
    void SetLayer(const std::string& layerName);
    void SetTargetLayers(const std::vector<std::string>& targetLayerNames);

    void ResetHitHistory() {if (collider_) collider_->ClearHitHistory();}
    void SetEnableHitHistory(bool enable) {if (collider_) collider_->SetEnableHitHistory(enable);}
private:
    // コライダーへの各種設定（UserData, Attribute, Mask, Callback）を一括適用する内部関数
    void SetupCollider();

private:
    std::unique_ptr<Collider> collider_;

    uint32_t attribute_ = 0xffffffff;
    uint32_t mask_ = 0xffffffff;
    Collider::CollisionCallBack callback_ = nullptr;
};