#pragma once
#include "ModelObject.h"

class GameMap {
public:
	void Initialize(Fngine* engine);
	void Update();
	void Draw();

private:
	std::unique_ptr<ModelObject> obj_;
	std::unique_ptr<BVH>bvh_;

public:
	BVH* GetBVH() { return bvh_.get(); }
};