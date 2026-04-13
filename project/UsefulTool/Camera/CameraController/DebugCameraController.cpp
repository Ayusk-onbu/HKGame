#include "CameraController.h"
#include "Camera.h"
#include "InputManager.h"
#include "ImGuiManager.h"

#include <algorithm>

void DebugCameraController::Update(Camera& camera)
{
	float panSpeed = 0.005f * camera.GetRadius();

	//   Zoom
	camera.GetRadius() -= InputManager::GetMouse().GetWheel() * panSpeed * 0.1f;

	//   視点の移動
	if (InputManager::GetKey().PressKey(DIK_LSHIFT) && InputManager::GetMouse().IsButtonPress(2)) {
		Vector3 right = camera.CalculateRight();
		Vector3 up = camera.CalculateUp();
		camera.targetPos_.x -= right.x * InputManager::GetMouse().getDelta().x * panSpeed;
		camera.targetPos_.x += up.x * InputManager::GetMouse().getDelta().y * panSpeed; // Y軸は反転
		camera.targetPos_.y -= right.y * InputManager::GetMouse().getDelta().x * panSpeed;
		camera.targetPos_.y += up.y * InputManager::GetMouse().getDelta().y * panSpeed; // Y軸は反転
		camera.targetPos_.z -= right.z * InputManager::GetMouse().getDelta().x * panSpeed;
		camera.targetPos_.z += up.z * InputManager::GetMouse().getDelta().y * panSpeed; // Y軸は反転
	}

	//   視点の回転
	if (InputManager::GetMouse().IsButtonPress(2) && !InputManager::GetKey().PressKey(DIK_LSHIFT)) {
		camera.GetTheta() += (InputManager::GetMouse().getDelta().y * 0.1f);
		camera.GetPhi() -= (InputManager::GetMouse().getDelta().x * 0.1f);
	}

	float theta = 0;
	camera.GetTheta() = std::clamp(camera.GetTheta(), -89.0f, 89.0f); // X軸回転は-90度から90度まで
	theta = Deg2Rad(camera.GetTheta());
	float phi = 0;
	phi = Deg2Rad(camera.GetPhi());

	// ここから変換
	camera.GetTranslation().x = camera.targetPos_.x + camera.GetRadius() * std::cos(theta) * std::cos(phi);
	camera.GetTranslation().y = camera.targetPos_.y + camera.GetRadius() * std::sin(theta);
	camera.GetTranslation().z = camera.targetPos_.z + camera.GetRadius() * std::cos(theta) * std::sin(phi);
	//viewProjectionMatrix_ = Matrix4x4::Inverse(Matrix4x4::Make::Affine(scale_, rotation_, translation_));
	camera.GetViewProjectionMatrix() = (Matrix4x4::Make::LookAt(camera.GetTranslation(), camera.targetPos_, camera.up_, camera.xAxis_, camera.yAxis_, camera.zAxis_));
	
	camera.GetViewProjectionMatrix() = Matrix4x4::Multiply(camera.GetViewProjectionMatrix(), Matrix4x4::Make::PerspectiveFov(
		camera.GetProjection().fovY, camera.GetProjection().aspectRatio,
		camera.GetProjection().nearClip, camera.GetProjection().farClip));

	ImGuiManager::GetInstance()->Text("camera");
	ImGuiManager::GetInstance()->DrawDrag("xAxis", camera.xAxis_);
	ImGuiManager::GetInstance()->DrawDrag("yAxis", camera.yAxis_);
	ImGuiManager::GetInstance()->DrawDrag("zAxis", camera.zAxis_);
	ImGuiManager::GetInstance()->DrawDrag("radius", camera.GetRadius());
	ImGuiManager::GetInstance()->DrawDrag("theta", camera.GetTheta());
	ImGuiManager::GetInstance()->DrawDrag("phi", camera.GetPhi());
}