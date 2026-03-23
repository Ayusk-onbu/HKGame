#pragma once
class Player;

class InputHandler {
public:
	InputHandler() = default;
public:
	void SetPlayer(Player* player) { player_ = player; }
	void HandleInput();
private:
	Player* player_;
};