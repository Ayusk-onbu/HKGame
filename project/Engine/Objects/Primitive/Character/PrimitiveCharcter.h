#pragma once
#include "Structures.h"
#include "../Base/PrimitiveBaseModel.h"

// --- GPU転送用構造体 ---
struct CharacterForGPU {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Matrix4x4 uvTransform;
    Vector4 color;
    uint32_t colorTextureIndex;  // カラーテクスチャのインデックス
    uint32_t normalTextureIndex; // 法線テクスチャのインデックス
	uint32_t skinMask1TextureIndex; // スキンマスク1のインデックス
	uint32_t skinMask2TextureIndex; // スキンマスク2のインデックス

    float melaninScale;
    float bloodVolumeScale;
    float roughnessScale;
    float sssScale;
};

// --- CPU側インスタンスデータ ---
struct CharacterObjectData {
    WorldTransform worldTransform;
    WorldTransform uvTransform;
    Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    std::string colorTextureName = "GridLine";
    std::string normalTextureName = "GridLine";
    std::string modelName = "PlayerModel";
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};// スキニング対応
    bool useCustomVB = false;// 使用するかどうか
    std::string skinMask1TextureName = "Masks_Melanin_Blood_Ratio_SSS.png";
    std::string skinMask2TextureName = "Masks_Melanin_Blood_Ratio_SSS.png";
	float melaninScale = 0.3f;
	float bloodVolumeScale = 0.1f;
	float roughnessScale = 1.0f;
	float sssScale = 1.0f;
};

class PrimitiveCharacter : public PrimitiveBaseModel<CharacterForGPU, CharacterObjectData> {
public:
    void Initialize(Fngine* engine, uint32_t numInstance) override;

private:
    CharacterForGPU ConvertToGPUData(const CharacterObjectData& data) override;

    std::string useModelName_ = "PlayerModel";
};