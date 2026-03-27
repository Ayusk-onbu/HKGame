#pragma once
#include "Structures.h"
#include "VertexData.h"
#include <vector>

struct SupportPoint {
	Vector2 v;// 差分ベクトル(ShapeB - ShapeA)
	Vector2 supA;// 各形状のサポートポイント(後で衝突点計算に使う場合)
	Vector2 supB;
};

struct Contact {
	Vector2 direction;
	float depth;
};

Vector2 support(std::vector<Vector2>vertices, const Vector2& direction);

float Cross2d(const Vector2& v1, const Vector2& v2);

float Dot2d(const Vector2& v1, const Vector2& v2);

SupportPoint getSupport(std::vector<Vector2>shapeA, std::vector<Vector2>shapeB, const Vector2& direction);

Vector2 getPerpendicularToOrigin(Vector2 edge, Vector2 toOrigin);

Vector2 GetOriginProjection(const Vector2& v1, const Vector2& v2);

float GetDistanceToOrigin(const Vector2& point);

Vector2 Normalize(const Vector2& v);

bool UpdateSimplex(std::vector<SupportPoint>& simplex, Vector2& direction);

Contact EPA(std::vector<Vector2>shapeA, std::vector<Vector2>shapeB, std::vector<SupportPoint>& simplex);

bool collision(std::vector<Vector2> shapeA, std::vector<Vector2>shapeB, std::vector<SupportPoint>& simplex);