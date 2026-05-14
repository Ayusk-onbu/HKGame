#pragma once
#include "WorldTransform.h"

struct PrimitiveBaseModelData {
	WorldTransform transform;
	std::string name;
};

struct PrimitiveForGPU {
	Matrix4x4 WVP;
	Matrix4x4 World;
	Vector4 color;
};