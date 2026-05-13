#pragma once
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

struct VertexData {
	Vector4 position;
	Vector2 texcoord;//u座標とv座標のことかな
	Vector3 normal;//法線ベクトル
};

struct SkyBoxVertexData {
	Vector4 position;
};