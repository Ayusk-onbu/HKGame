#pragma once
#include "../../Character.h"

class MagicProjectile : public Character {
public:
	MagicProjectile() = default;
	~MagicProjectile()override = default;

public:
	void Initialize(Fngine* engine, std::string modelName = "cube", std::string textureName = "GridLine")override;
	void InitializeInformation(MagicData magicData);
	void Update(float deltaTime)override;
	void Draw()override;

private:
	void OnHit(Collider* collider, const Vector3& outPush);
	void Destroy();

private:
	ProjectileData projData_;
};