#pragma once
#include "Component.h"
#include "SDFStackType.h"

class SDFFunctionComponent : 
	public Component 
{
public:
    SDFFunctionComponent() = default;
    ~SDFFunctionComponent() override { OnDisable(); };

    void Initialize() override;
    void Update(float deltaTime) override { ResolveBoneBindings(); }
    void DrawUI() override;

    void OnEnable();
    void OnDisable();

    void AddNode(const SDFFunctionStackNodeCPU& node) {
        nodes_.push_back(node);
        RecalculateAABB();
    }

    // スキニング対応させるため
    void ResolveBoneBindings();

    std::string GetComponentName() const override { return "SDFFunctionComponent"; }
    json Serialize() const override;
    void Deserialize(const json& j) override;
    bool LoadFromFile(const std::string& filePath);

    void ClearNodes() {
        nodes_.clear();
        localAABBMin_ = Vector3(0.0f, 0.0f, 0.0f);
        localAABBMax_ = Vector3(0.0f, 0.0f, 0.0f);
    }

    const std::vector<SDFFunctionStackNodeCPU>& GetNodes() const { return nodes_; }
    Vector3 GetLocalAABBMin() const { return localAABBMin_; }
    Vector3 GetLocalAABBMax() const { return localAABBMax_; }
    void GetCurrentLocalAABB(Vector3& outMin, Vector3& outMax) const;
private:
    void RecalculateAABB();

private:
    std::vector<SDFFunctionStackNodeCPU> nodes_;
    Vector3 localAABBMin_ = { 0.0f, 0.0f, 0.0f };
    Vector3 localAABBMax_ = { 0.0f, 0.0f, 0.0f };
};