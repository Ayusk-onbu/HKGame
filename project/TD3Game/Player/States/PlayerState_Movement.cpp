#include "../PlayerStates.h"
#include "../Player.h"
#include "MathUtils.h"

namespace PlayerStates::Movement {
	////////////////////////////
	//
	//  Grounded
	// 
	////////////////////////////
	void Grounded::Update(float deltaTime) {
		//// 地上にいる際の処理 ////

		// 地上の摩擦係数(大きいほどすぐ止まる)
		float groundFriction = 10.0f;

		// 外部からの速度の減衰関係の処理
		// ※ 自発的な速度はIdleで減衰させる(Walk中はしなくていいため)
		player_->externalVelocity_.x = Lerp(player_->externalVelocity_.x, 0.0f, groundFriction * deltaTime);
		//player_->externalVelocity_.z = Lerp(player_->externalVelocity_.z, 0.0f, groundFriction * deltaTime);

		// 重力の処理(地面にいるからする意味はないが、一応する)
		// ※ 関数でまとめておく

		// Y軸の速度
		player_->externalVelocity_.y = 0.0f;// ※地面にいるので 0

		// ジャンプ
		player_->Jump();
	}
	////////////////////////////
	//
	//  Airborne
	// 
	////////////////////////////
	/*void Airborne::Enter() {

	}*/

	void Airborne::Update(float deltaTime) {
		// 空気の抵抗（小さいのであまり減速しない）
		float airResistance = 1.0f;
		float gravity = 9.8f; // 重力加速度

		// 外部からの速度（X, Z軸）を少しだけ減衰させる
		player_->externalVelocity_.x = std::lerp(player_->externalVelocity_.x, 0.0f, airResistance * deltaTime);
		player_->externalVelocity_.z = std::lerp(player_->externalVelocity_.z, 0.0f, airResistance * deltaTime);

		// Y軸には常に重力をかけ続ける
		player_->externalVelocity_.y -= gravity * deltaTime;

		if (player_->GetPosition().y <= 0.0f) {

			// 地面にめり込まないようにY座標を0に補正
			Vector3 pos = player_->GetPosition();
			pos.y = 0.0f;
			player_->SetPosition(pos);

			// 落下速度をリセット
			player_->externalVelocity_.y = 0.0f;

			// 地上に着いたので Idle ステートに戻す！
			player_->ChangeMovementState(player_->idleState_.get());
		}
	}

	/*void Airborne::Exit() {

	}*/
	////////////////////////////
	//
	//  Idle
	// 
	////////////////////////////
	void Idle::Enter() {

	}

	void Idle::Update(float deltaTime) {
		if (parentState_) {
			parentState_->Update(deltaTime);
		}
		const auto& input = player_->GetInput();

		// 入力方向がゼロじゃない（スティックが倒された）なら、Walkingへ遷移
		if (input.moveDirection.x != 0.0f || input.moveDirection.z != 0.0f) {
			player_->ChangeMovementState(player_->walkingState_.get());
			return; // 遷移したらこのフレームの処理は終了
		}

		// スティックが倒されていないなら、自発的な速度(myVelocity_)を摩擦でゼロに近づける
		float deceleration = 15.0f; // ブレーキの強さ
		player_->myVelocity_.x = std::lerp(player_->myVelocity_.x, 0.0f, deceleration * deltaTime);
		player_->myVelocity_.z = std::lerp(player_->myVelocity_.z, 0.0f, deceleration * deltaTime);
	}

	void Idle::Exit() {

	}
	////////////////////////////
	//
	//  Walking
	// 
	////////////////////////////
	void Walking::Enter() {

	}

	void Walking::Update(float deltaTime) {
		if (parentState_) {
			parentState_->Update(deltaTime);
		}
		const auto& input = player_->GetInput();

		// スティックが離されたら、Idleへ遷移！
		if (input.moveDirection.x == 0.0f && input.moveDirection.z == 0.0f) {
			player_->ChangeMovementState(player_->idleState_.get());
			return;
		}

		float targetSpeed = 0.0f;

		// 武器の状態で切り替え
		switch (player_->GetWeaponStance()) {
		case WeaponStance::Sheathed:
			targetSpeed = 5.0f;
			break;
		case WeaponStance::Drawn:
			targetSpeed = 3.0f;
			break;
		}

		float acceleration = 15.0f;
		float targetVelocityX = input.moveDirection.x * targetSpeed;
		//float targetVelocityZ = player_->moveDirection_.z * targetSpeed;今回はいらない

		// 目標速度に加速させる
		player_->myVelocity_.x = Lerp(player_->myVelocity_.x, targetVelocityX, acceleration * deltaTime);
		//player_->myVelocity_.z = Lerp(player_->myVelocity_.z, targetVelocityZ, acceleration * deltaTime);

		// もし、トップスピードになったらRunningに移行
		// ※もしキャラクターのモデルの向きを移動方向に合わせるなら、ここに書く
	}

	void Walking::Exit() {

	}
}