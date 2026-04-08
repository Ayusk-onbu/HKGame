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

	float rightStickX = InputManager::GetGamePad(0).GetRightStickX();
	float rightStickY = InputManager::GetGamePad(0).GetRightStickY();
	input.aimingDirectionX = (std::abs(rightStickX) > 0.15f) ? rightStickX : 0.0f;
	input.aimingDirectionY = (std::abs(rightStickY) > 0.15f) ? rightStickY : 0.0f;

    if (InputManager::GetKey().PressKey(DIK_W)) input.moveDirection.z += 1.0f;
    if (InputManager::GetKey().PressKey(DIK_S)) input.moveDirection.z -= 1.0f;
    if (InputManager::GetKey().PressKey(DIK_A)) input.moveDirection.x -= 1.0f;
    if (InputManager::GetKey().PressKey(DIK_D)) input.moveDirection.x += 1.0f;

    if (input.moveDirection.x != 0.0f) {
        player_->eyesDirection_.x = input.moveDirection.x;
    }

    // --- アクション入力の取得 ---
    input.isJump = InputManager::IsJump();
    // ==================
    // 【 アタック系 】
    // ==================
    input.isAttack = InputManager::IsAttack();
    
    input.isAttackHeld = false;
    if (InputManager::GetKey().HoldKey(DIK_J)) {
        input.isAttackHeld = true;
    }
    if (InputManager::GetGamePad(0).IsHold(XINPUT_GAMEPAD_Y)) {
        input.isAttackHeld = true;
    }

    input.isAttackReleased = false;
    if (InputManager::GetKey().ReleaseKey(DIK_J)) {
        input.isAttackReleased = true;
    }
    if (InputManager::GetGamePad(0).IsRelease(XINPUT_GAMEPAD_Y)) {
        input.isAttackReleased = true;
    }

    input.isEvasion = InputManager::TrigerEvasion();
    input.isSheathe = false; // ※任意のボタンを設定（例: IsSheathe() など）
    input.isGuard = false;
    input.isReverse = false;

    // 納刀 -> 回復がスムーズに入力出来る
    if (InputManager::GetKey().PressedKey(DIK_RETURN)) {
        input.isSheathe = true;
    }
    if (InputManager::GetGamePad(0).IsPressed(XINPUT_GAMEPAD_X)) {
        input.isShoot = true;
    }

    if (InputManager::GetKey().PressedKey(DIK_I)) {
        input.isGuard = true;
    }
    if (InputManager::GetKey().PressedKey(DIK_O)) {
        input.isReverse = true;
    }

    input.useMana = false;
    // マナ使用モードとして実装するかどうか
    if (InputManager::GetKey().PressKey(DIK_LSHIFT)) {
        input.useMana = true;
    }

    // ================
    // 【 照準・発射 】
    // ================
	input.isAiming = false; // 照準を合わせているかどうか
    input.isAimingHeld = false;
	input.isShoot = false;  // 射撃したかどうか
    if (InputManager::GetGamePad(0).IsPressed(XINPUT_GAMEPAD_LEFT_SHOULDER)) {
        input.isAiming = true;
    }
    if (InputManager::GetGamePad(0).IsHold(XINPUT_GAMEPAD_LEFT_SHOULDER)) {
        input.isAimingHeld = true;
    }
    if (InputManager::GetGamePad(0).IsPressed(XINPUT_GAMEPAD_RIGHT_SHOULDER)) {
        input.isShoot = true;
    }

    // ================
    // 【 修復 】
    // ================
    input.isRepair = false;
    if (InputManager::GetKey().PressedKey(DIK_R)) {
        input.isRepair = true;
    }
    if (InputManager::GetGamePad(0).IsPressed(XINPUT_GAMEPAD_X)) {
        input.isShoot = true;
    }

    // Playerに入力情報を渡す！
    player_->SetInputData(input);
}