#pragma once
#include "DynamicObject.h"
#include "InputHandler.h"
#include "States.h"

class Character : public DynamicObject
{
public:
	Character() = default;
	~Character()override = default;

///////////////////////////
/// 
/// 基本的な存在
///
//////////////////////////
public:
	void Initialize(Fngine* engine, std::string modelName = "cube", std::string textureName = "GridLine")override;
	/// <summary>
	/// DynamicObjectの更新処理に加えて、等の更新処理
	/// </summary>
	/// <param name="deltaTime"></param>
	void Update(float deltaTime)override;
	void Draw()override;

protected:

///////////////////////////
/// 
/// 物理的な存在達
///
//////////////////////////
public:
	void Jump();
	void SetEyesDirection(const Vector3& eyesDirection) { eyesDirection_ = eyesDirection; }
	Vector3 GetEyesDirection()const { return eyesDirection_; }
	
protected:
	// 移動制御用の変数
	Vector3 eyesDirection_;// 視線の方向

///  *================*
/// 【 操作感に関する 】
///  *================*
public:
	float GetCoyoteTimer()const { return jumpCoyoteTimer_; }
	void SetCoyoteTimer(float time) { jumpCoyoteTimer_ = time; }
	void ResetCoyoteTime() { jumpCoyoteTimer_ = JUMP_COYOTE_MAX_TIME; }
	void DecreaseCoyoteTimer(float deltaTime) { jumpCoyoteTimer_ -= deltaTime; }

protected:
	float jumpCoyoteTimer_ = 0.0f;// 地面を離れた後もジャンプを受け付ける猶予期間
	const float JUMP_COYOTE_MAX_TIME = 0.15f;

///////////////////////////
/// 
/// 操作に関する関数(エルミートや操作での操作)
///
//////////////////////////
public:
	const CommandState& GetCommand() const { return input_; }

protected:
	std::unique_ptr<Controller>controller_;
	CommandState input_{};


///////////////////////////
/// 
/// 状態に関する関数
///
//////////////////////////
public:
	/// <summary>
	/// 状態の変更
	/// </summary>
	/// <param name="stateName">変更したいStateの名前</param>
	void ChangeMovementState(std::string stateName);

public:
	// 移動に関する状態の取得
	CharacterState::Base* GetMovementCurrentState() const { return mm_currentState_; }
	// アクションに関する状態の取得
	CharacterState::Base* GetActionCurrentState() const { return ac_currentState_; }

protected:
	CharacterState::Base* mm_currentState_;
	CharacterState::Base* ac_currentState_;
	struct {
		std::unique_ptr<CharacterState::Movement::GroundBase> groundBase_;
		std::unique_ptr<CharacterState::Movement::AirBase> airBase_;
		std::unique_ptr<CharacterState::Movement::Restricted> restrictedBase_;
		std::unique_ptr<CharacterState::Movement::Idle> idleBase_;
		std::unique_ptr<CharacterState::Movement::Walk> walk_;
	}mm_states_;

private:
	void InitializeStates();
};

