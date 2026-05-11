#pragma once
#include "Vector3.h"

enum class ButtonState {
	None,    // 0 - 0
	Pressed, // 0 - 1
	Held,    // 1 - 1
	Released,// 1 - 0
};

inline bool IsButtonDown(ButtonState input) {
	return input == ButtonState::Pressed || input == ButtonState::Held;
}

inline bool IsButtonUp(ButtonState input) {
	return input == ButtonState::None || input == ButtonState::Released;
}

inline ButtonState UpdateButtonState(ButtonState previousState, bool isPress) {
	if (isPress) {
		// 今押されていて、前回押されていなかったら「押した瞬間」
		if (previousState == ButtonState::None || previousState == ButtonState::Released) {
			return ButtonState::Pressed;
		}
		// それ以外（前回も押されていた）なら「押しっぱなし」
		return ButtonState::Held;
	}
	else {
		// 今押されてなくて、前回押されていたら「離した瞬間」
		if (previousState == ButtonState::Pressed || previousState == ButtonState::Held) {
			return ButtonState::Released;
		}
		// それ以外（前回も押されてない）なら「何もなし」
		return ButtonState::None;
	}
}

struct CommandState {
	Vector3 moveDirection{};                          // 移動する方向(左スティック)
	ButtonState dash{ ButtonState::None };            // ダッシュ(L3)
												     
	Vector3 cameraDirection{};                        // カメラの回転方向(ショートカットの選択も)(右スティック)
	ButtonState changeTarget{ ButtonState::None };    // ターゲット切り替え(R3)
												     
	ButtonState mainAction{ ButtonState::None };      // メインアクション(抜刀・通常攻撃)(Yボタン)
	ButtonState subAction{ ButtonState::None };       // サブアクション(採取や話しかける等のアクション・特殊攻撃)(Bボタン)
	ButtonState subMove{ ButtonState::None };         // サブ移動(回避か、ジャンプ)(Aボタン)
	ButtonState magicAction{ ButtonState::None };     // 魔法アクション(納刀・魔法発動)(Xボタン)
												     
	ButtonState changeMagic{ ButtonState::None };     // 魔法切替(ショートカット発動も含む ターゲットに向く)(L1)
	ButtonState aiming{ ButtonState::None };          // 照準(L2)
												     
	ButtonState changeWeapon{ ButtonState::None };    // 武器切り替え(ショートカット)ターゲットに向く(R1)
	ButtonState specialAction{ ButtonState::None };   // 特殊アクション(ガード・照準時発射・特殊攻撃)(R2)

	ButtonState changeUp{ ButtonState::None };        // 上切り替え(決定ボタンになるかも)(十字キー上)
	ButtonState switchSpiritNext{ ButtonState::None };// 右切り替え(霊の順送り)(十字キー右)
	ButtonState changeDown{ ButtonState::None };      // 下切り替え(十字キー下)
	ButtonState switchSpiritPrev{ ButtonState::None };// 左切り替え(霊の逆送り)(十字キー左)
};

class Controller {
public:
	virtual ~Controller() = default;
	virtual CommandState GetCommandState(CommandState preState) = 0;
};

class PlayerController : public Controller
{
public:
	~PlayerController() = default;
	CommandState GetCommandState(CommandState preState)override;
};
