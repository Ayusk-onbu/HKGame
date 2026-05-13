#pragma once
#include "Quaternion.h"
#include "Transform.h"
#include "Matrix4x4.h"

class WorldTransform
{
public:
	struct Get {
		WorldTransform* parent_;
		Vector3& Scale();
		Vector3& Rotation();
		Vector3& Translation();
		Quaternion& Quaternion();
		Vector3 ForwardVector();
	};
	struct Set {
		WorldTransform* parent_;
		void Scale(const Vector3& scale);
		void Rotation(const Vector3& rotation);
		void Translation(const Vector3& translation);
		void Quaternion(const Quaternion& q) {
			parent_->quaternion_ = q;
			if (parent_->isDirty_ == false) {
				parent_->isDirty_ = true;
			}
		};
	};
public:
	void Initialize();
	void LocalToWorld();
	const Vector3 GetWorldPos()const;
	void LookAtToTarget(const Vector3& v);
	void LookAtToDirection(const Vector3& v);
public:
	Get get_;
	Set set_;

	Matrix4x4* parent_ = nullptr;
	Transform transform_;
	Matrix4x4 mat_;
	bool isDirty_ = false;
	Quaternion quaternion_;
};