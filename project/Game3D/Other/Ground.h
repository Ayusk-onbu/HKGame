#pragma once
#include "ModelObject.h"
#include "Animation.h"
#include "Collider.h"

class Ground
{
public:
	void Initialize(Fngine* fngine);
	void Update();
	void Draw();

	//////////////////////////
	///
	///   当たり判定
	///
	//////////////////////////
public:
	Collider* GetCollider()const { return collider_.get(); }
	void SetVertices(const std::vector<Vector3>vertices){ collider_->SetVertices(vertices); }
private:
	std::unique_ptr<ConvexCollider> collider_;

private:
	std::unique_ptr<ModelObject>obj_ = nullptr;
};















class ConvenienceModel
{
public:
	void Initialize(Fngine* fngine, const std::string& modelName, const std::string& textureName);
	void Update();
	void Draw();
public:
	std::unique_ptr<ModelObject>obj_ = nullptr;
};

class AnimModel
{
public:
	void Initialize(Fngine* fngine, const std::string& modelName, const std::string& textureName,const std::string& filePath, const std::string& fileName);
	void Update();
	void Draw();
	void DrawImGui();
public:
	std::unique_ptr<ModelObject>obj_ = nullptr;
	std::unique_ptr<Animation>animation_ = nullptr;
	std::unique_ptr<Skeleton>skeleton_ = nullptr;
};

class Anim2Model
{
public:
	void Initialize(Fngine* fngine, const std::string& modelName, const std::string& textureName, const std::string& filePath, const std::string& fileName);
	void Update();
	void Draw();
	void DrawImGui();
public:
	std::unique_ptr<ModelObject>obj_ = nullptr;
	std::unique_ptr<Animation>animation_ = nullptr;
	std::unique_ptr<Skeleton>skeleton_ = nullptr;
};

