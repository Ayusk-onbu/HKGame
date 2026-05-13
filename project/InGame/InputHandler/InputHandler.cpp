#include "InputHandler.h"
#include "InputManager.h"
#include <dinput.h>
#include <cmath>

CommandState PlayerController::GetCommandState(CommandState preState) {
	auto key = InputManager::GetKey();               // 非 const コピーを取得して非 const メンバを呼べるようにする
	auto gamepad = InputManager::GetGamePad(0);

	CommandState cmd;

	//   =============
	// 【 Lスティック 】
	//   =============
	float stickX = gamepad.GetLeftStickX();
	float stickY = gamepad.GetLeftStickY();

	// キーボードの入力
	float keyboardX = 0.0f;
	if (key.PressKey(DIK_D)) keyboardX += 1.0f;
	if (key.PressKey(DIK_A)) keyboardX -= 1.0f;

	float keyboardY = 0.0f;
	if (key.PressKey(DIK_W)) keyboardY += 1.0f;
	if (key.PressKey(DIK_S)) keyboardY -= 1.0f;

	Vector3 normalKeyboardInput = Normalize({ keyboardX,keyboardY,0.0f });

	// 入力を合成して最終的な移動量を計算
	// スティックとキーボードで値が大きい方を採用
	float moveX = (std::abs(stickX) > 0.1f) ? stickX : normalKeyboardInput.x;
	float moveY = (std::abs(stickY) > 0.1f) ? stickY : normalKeyboardInput.y;

	// 4. 指令値（cmd）へ代入
	cmd.moveDirection.x = moveX;
	cmd.moveDirection.z = moveY;

	cmd.dash = UpdateButtonState(preState.dash, (key.PressKey(DIK_LSHIFT) || gamepad.GetLeftTrigger() > 50));

	//   =============
	// 【 Rスティック 】
	//   =============
	stickX = gamepad.GetRightStickX();
	stickY = gamepad.GetRightStickY();

	if (std::abs(stickX) > 0.1f) cmd.cameraDirection.x = stickX;
	if (std::abs(stickY) > 0.1f) cmd.cameraDirection.z = stickY;

	cmd.changeTarget = UpdateButtonState(preState.changeTarget, (key.PressKey(DIK_LALT) || gamepad.GetRightTrigger() > 50));

	// YBAX
	cmd.mainAction = UpdateButtonState(preState.mainAction, (key.PressKey(DIK_Y) || gamepad.IsPress(XINPUT_GAMEPAD_Y)));
	cmd.subAction = UpdateButtonState(preState.subAction, (key.PressKey(DIK_Y) || gamepad.IsPress(XINPUT_GAMEPAD_B)));
	cmd.subMove = UpdateButtonState(preState.subMove, (key.PressKey(DIK_Y) || gamepad.IsPress(XINPUT_GAMEPAD_A)));
	cmd.magicAction = UpdateButtonState(preState.magicAction, (key.PressKey(DIK_Y) || gamepad.IsPress(XINPUT_GAMEPAD_X)));

	// L
	cmd.changeMagic = UpdateButtonState(preState.changeMagic, (key.PressKey(DIK_Y) || gamepad.IsPress(XINPUT_GAMEPAD_LEFT_SHOULDER)));
	cmd.aiming = UpdateButtonState(preState.aiming, (key.PressKey(DIK_Y) || gamepad.IsPress(XINPUT_GAMEPAD_LEFT_THUMB)));

	// R
	cmd.changeWeapon = UpdateButtonState(preState.changeWeapon, (key.PressKey(DIK_Y) || gamepad.IsPress(XINPUT_GAMEPAD_RIGHT_SHOULDER)));
	cmd.specialAction = UpdateButtonState(preState.specialAction, (key.PressKey(DIK_Y) || gamepad.IsPress(XINPUT_GAMEPAD_RIGHT_THUMB)));

	//   ============
	// 【　十字キー　】
	//   ============
	cmd.changeUp = UpdateButtonState(preState.changeUp, (key.PressKey(DIK_Y) || gamepad.IsPress(XINPUT_GAMEPAD_DPAD_UP)));
	cmd.switchSpiritNext = UpdateButtonState(preState.switchSpiritNext, (key.PressKey(DIK_Y) || gamepad.IsPress(XINPUT_GAMEPAD_DPAD_RIGHT)));
	cmd.changeDown = UpdateButtonState(preState.changeDown, (key.PressKey(DIK_Y) || gamepad.IsPress(XINPUT_GAMEPAD_DPAD_DOWN)));
	cmd.switchSpiritPrev = UpdateButtonState(preState.switchSpiritPrev, (key.PressKey(DIK_Y) || gamepad.IsPress(XINPUT_GAMEPAD_DPAD_LEFT)));

	return cmd;
}