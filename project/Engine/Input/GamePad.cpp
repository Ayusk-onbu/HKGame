#include "GamePad.h"
#include "ImGuiManager.h"

#pragma comment(lib, "Xinput.lib")

GamePad::GamePad(int index)
    : controllerIndex_(index), isConnected_(false)
{
    ZeroMemory(&state_, sizeof(XINPUT_STATE));
}

void GamePad::Update() {
    memcpy(&prevState_, &state_, sizeof(XINPUT_STATE));
    ZeroMemory(&state_, sizeof(XINPUT_STATE));
    DWORD result = XInputGetState(controllerIndex_, &state_);
    isConnected_ = (result == ERROR_SUCCESS);
#ifdef USE_IMGUI
    ImGui::Begin("GamePad");
	float rightStickX = static_cast<float>(GetRightStickX());
	ImGui::DragFloat("RightStickX", &rightStickX, 0.01f, -1.0f, 1.0f);
    float leftStickX = GetLeftStickX();
    ImGui::DragFloat("LeftStickX", &leftStickX, 0.01f, -1.0f, 1.0f);
    ImGui::End();
#endif//_DEBUG
}

bool GamePad::IsConnected() const {
    return isConnected_;
}

bool GamePad::IsPress(WORD button) {
    if (!isConnected_) return false;
    return (state_.Gamepad.wButtons & button) != 0;
}

bool GamePad::IsPressed(WORD button) {
    if (!isConnected_) return false;
    return ((state_.Gamepad.wButtons & button) != 0) &&
        ((prevState_.Gamepad.wButtons & button) == 0);
}

bool GamePad::IsHold(WORD button) const {
    if (!isConnected_) return false;
    return ((state_.Gamepad.wButtons & button) != 0) &&
        ((prevState_.Gamepad.wButtons & button) != 0);
}

bool GamePad::IsRelease(WORD button) const {
    if (!isConnected_) return false;
    return ((state_.Gamepad.wButtons & button) == 0) &&
        ((prevState_.Gamepad.wButtons & button) != 0);
}

BYTE GamePad::GetLeftTrigger() const {
    return isConnected_ ? state_.Gamepad.bLeftTrigger : 0;
}

float GamePad::GetLeftStickX() const {
    return isConnected_ ? static_cast<float>(state_.Gamepad.sThumbLX / 32767.0f) : 0;
}

float GamePad::GetLeftStickY() const {
    return isConnected_ ? static_cast<float>(state_.Gamepad.sThumbLY / 32767.0f) : 0;
}

float GamePad::GetRightStickX() const {
    return isConnected_ ? static_cast<float>(state_.Gamepad.sThumbRX / 32767.0f) : 0;
}

float GamePad::GetRightStickY() const {
    return isConnected_ ? static_cast<float>(state_.Gamepad.sThumbRY / 32767.0f) : 0;
}

BYTE GamePad::GetRightTrigger() const {
    return isConnected_ ? state_.Gamepad.bRightTrigger : 0;
}
