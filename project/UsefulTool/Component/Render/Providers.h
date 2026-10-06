#pragma once
#include "Engine/Objects/DrawManager.h"
#include "TextureManager.h"
#include "WorldTransform.h"

class DynamicObject;

// --- 描画データのインターフェース ---
class IRenderProvider {
public:
    virtual ~IRenderProvider() = default;
    virtual void SendToDrawManager(DynamicObject* owner) = 0;
    virtual void DrawUI() {}
    virtual json Serialize() const { return json::object(); }
    virtual void Deserialize(const json& j) {}
};

class GrassRenderProvider : public IRenderProvider {
public:
    GrassObjectData data;

    void SendToDrawManager(DynamicObject* owner) override;

    void DrawUI() override;
};

class CharacterRenderProvider : public IRenderProvider {
public:
    CharacterObjectData data;

    void SendToDrawManager(DynamicObject* owner) override;

    void DrawUI() override;
};

class NormalMeshRenderProvider : public IRenderProvider {
public:
    NormalMeshObjectData data;

    void SendToDrawManager(DynamicObject* owner) override;

    void DrawUI() override;

    json Serialize() const override;

    void Deserialize(const json& j) override;
};