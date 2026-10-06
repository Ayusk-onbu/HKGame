#pragma once
#include "Component.h"
#include "Providers.h"

// --- 共通RenderComponent ---
class RenderComponent : public Component {
public:
    template <typename T, typename... Args>
    T* SetProvider(Args&&... args) {
        auto provider = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = provider.get();
        provider_ = std::move(provider);
        return ptr;
    }
    void Update(float deltaTime)override;
    void DrawUI()override;
    std::string GetComponentName() const override { return "Render"; }
    json Serialize() const override;
    void Deserialize(const json& j) override;

private:
    std::unique_ptr<IRenderProvider> provider_;

    // ------------------
    // SDF関係のコード
    // ------------------
public:

    void SetCastSDFShadow(bool enable) { castSDFShadow_ = enable; }
    bool IsCastSDFShadowEnable()const { return castSDFShadow_; }

    void SendToDrawManager();

private:
    uint32_t modelID_ = 0;
    bool castSDFShadow_ = true;
};