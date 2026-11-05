#pragma once
#include "Component.h"
#include "RayTracingManager.h"

class RayTracingComponent : public Component {
public:
    void Update(float deltaTime) override;
    void DrawUI() override;

    std::string GetComponentName() const override { return "RayTracingComponent"; }
    json Serialize() const override;
    void Deserialize(const json& j) override;

    void SetModelID(uint32_t modelID) { modelID_ = modelID; }
    void SetColor(const Vector4& color) { color_ = color; }

private:
    uint32_t modelID_ = 0;
    Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
};