#include "Camera.h"
#include "InputManager.h"
#include "ImGuiManager.h"
#include <algorithm>

Camera::Camera()
	: up_({ 0.0f, 1.0f, 0.0f })
	, theta_(0.0f)
	, phi_(270.0f)
	, radius_(10.0f)
	, xAxis_({ 1.0f,0.0f,0.0f})
	, yAxis_({ 0.0f,1.0f,0.0f })
	, zAxis_({ 0.0f,0.0f,1.0f })
	, projection_({0.45f,1280.0f / 720.0f,0.1f,1000.0f})
	, targetPos_({ 0.0f,0.0f,0.0f })
{}

void Camera::Initialize() {
	scale_ = { 1.0f,1.0f,1.0f };
	rotation_ = { 0.0f,0.0f,0.0f };
	translation_ = { 0.0f,10.0f,50.0f }; // カメラの位置
}

void Camera::Update() {
	for (int i = 0;i < controllers_.size();++i) {
		controllers_[i]->Update(*this);
	}
}

Matrix4x4 Camera::DrawCamera(const Matrix4x4& world) {
	Matrix4x4 WVP = Matrix4x4::Multiply(world, viewProjectionMatrix_);
	return WVP;
}

Matrix4x4 Camera::DrawUI(const Matrix4x4& world) {
	Matrix4x4 viewMat = Matrix4x4::Make::Identity();
	Matrix4x4 projectionMat = Matrix4x4::Make::OrthoGraphic(0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 100.0f);
	Matrix4x4 WVP = world * viewMat * projectionMat;
	return WVP;
}

Vector3 Camera::CalculateRight() {
	return Normalize(CrossProduct({ 0.0f,1.0f,0.0f }, Normalize(Subtract(targetPos_, translation_))));
}
Vector3 Camera::CalculateUp() {
	Vector3 forward = Normalize(Subtract(targetPos_, translation_));
	Vector3 right = CalculateRight();
	return Normalize(CrossProduct(forward, right));
}

void Camera::AddControllers(CameraType cameraTypes) {
	if ((cameraTypes & CameraType::Debug) == CameraType::Debug) {
		AddController(CameraType::Debug);
	}
	if ((cameraTypes & CameraType::Normal) == CameraType::Normal) {
		AddController(CameraType::Normal);
	}
	if ((cameraTypes & CameraType::Game) == CameraType::Game) {
		AddController(CameraType::Game);
	}
}

void Camera::AddController(CameraType cameraType) {
	// すでに同じタイプのコントローラーがあるか確認
	for (const auto& controller : controllers_) {
		if (controller->GetType() == cameraType) {
			return; // すでに追加済みなので何もしない
		}
	}

	// 新しいコントローラーを追加
	if (cameraType == CameraType::Debug) {
		controllers_.push_back(std::make_unique<DebugCameraController>());
	}
	else if (cameraType == CameraType::Normal) {
		controllers_.push_back(std::make_unique<NormalCameraController>());
	}
	else if (cameraType == CameraType::Game) {
		controllers_.push_back(std::make_unique<GameCameraController>());
	}
}