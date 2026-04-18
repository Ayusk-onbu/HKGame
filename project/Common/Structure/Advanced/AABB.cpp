#include "AABB.h"

#include <algorithm>
#include <cfloat>

AABB AABB::World2AABB(Vector3 world) {
	Vector3 worldPos = world;
	AABB aabb;

	aabb.min = { worldPos.x - 0.5f, worldPos.y - 0.5f, worldPos.z - 0.5f };
	aabb.max = { worldPos.x + 0.5f, worldPos.y + 0.5f, worldPos.z + 0.5f };
	aabb.center();

	return aabb;
}

bool AABB::IsHitAABB2AABB(const AABB& a, const AABB& b) {
	if (a.min.x <= b.max.x && b.min.x <= a.max.x && a.min.y <= b.max.y && b.min.y <= a.max.y && a.min.z <= b.max.z && b.min.z <= a.max.z) {
		return true;
	}
	return false;
}

Vector3 AABB::center() const {
	Vector3 ret;
	ret.x = (min.x + max.x) / 2.0f;
	ret.y = (min.y + max.y) / 2.0f;
	ret.z = (min.z + max.z) / 2.0f;
	return ret;
}

void AABB::Initialize() {
	min.x = -0.5f;
	min.y = -0.5f;
	min.z = -0.5f;

	max.x = 0.5f;
	max.y = 0.5f;
	max.z = 0.5f;
}

void AABB::Expand(const Vector3& point) {
	min.x = (std::min)(min.x, point.x);
	min.y = (std::min)(min.y, point.y);
	min.z = (std::min)(min.z, point.z);

	max.x = (std::max)(max.x, point.x);
	max.y = (std::max)(max.y, point.y);
	max.z = (std::max)(max.z, point.z);
}

void AABB::Expand(const AABB& other) {
	Expand(other.min);
	Expand(other.max);
}

Vector3 AABB::GetSize() const {
	return { max.x - min.x, max.y - min.y, max.z - min.z };
}

void AABB::ResetToExtreme() {
	min = { FLT_MAX, FLT_MAX, FLT_MAX };
	max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
}