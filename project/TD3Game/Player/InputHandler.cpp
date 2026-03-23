#include "InputHandler.h"
#include "InputManager.h"
#include "Player.h"

void InputHandler::HandleInput() {
	if (!player_)return;
	// 初期化
	PlayerInputData input;
	input.moveDirection = { 0.0f,0.0f,0.0f };
	// 移動入力を取得
    float stickX = InputManager::GetGamePad(0).GetLeftStickX();
    float stickY = InputManager::GetGamePad(0).GetLeftStickY();
    if (std::abs(stickX) > 0.15f) { input.moveDirection.x = stickX; }
    if (std::abs(stickY) > 0.15f) input.moveDirection.z = stickY;

    if (InputManager::GetKey().PressKey(DIK_W)) input.moveDirection.z += 1.0f;
    if (InputManager::GetKey().PressKey(DIK_S)) input.moveDirection.z -= 1.0f;
    if (InputManager::GetKey().PressKey(DIK_A)) input.moveDirection.x -= 1.0f;
    if (InputManager::GetKey().PressKey(DIK_D)) input.moveDirection.x += 1.0f;

    if (input.moveDirection.x != 0.0f) {
        player_->eyesDirection_.x = input.moveDirection.x;
    }

    // --- アクション入力の取得 ---
    input.isJump = InputManager::IsJump();
    input.isAttack = InputManager::IsAttack();
    input.isEvasion = InputManager::TrigerEvasion();
    input.isSheathe = false; // ※任意のボタンを設定（例: IsSheathe() など）

    // Playerに入力情報を渡す！
    player_->SetInputData(input);
}