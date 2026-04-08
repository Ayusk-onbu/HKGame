#pragma once
#include <windows.h>
#include <Xinput.h>

class GamePad {
public:
    GamePad(int index = 0);  // コントローラー番号（0〜3）
    ~GamePad() = default;
public:
    void Update();                 // 入力情報を更新
    bool IsConnected() const;     // 接続状態
    bool IsPress(WORD button);  // ボタン入力判定（A/B/Xなど）
    bool IsPressed(WORD button);  // トリガー入力判定
	bool IsHold(WORD button) const; // ボタンが押され続けているか
	bool IsRelease(WORD button) const; // ボタンが離されたか

    BYTE GetLeftTrigger() const;  // 左トリガーの値
    float GetLeftStickX() const;  // 左スティックのX軸
    float GetLeftStickY() const;  // 左スティックのY軸

    BYTE GetRightTrigger() const; // 右トリガーの値
	float GetRightStickX() const; // 右スティックのX軸
	float GetRightStickY() const; // 右スティックのY軸
	

private:
    int controllerIndex_;
    XINPUT_STATE state_;
    XINPUT_STATE prevState_;
    bool isConnected_;
};
