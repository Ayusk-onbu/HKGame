#include "../PlayerStates.h"
#include "../Player.h"

namespace PlayerStates::Action {
	////////////////////////////
	//
	//  Normal
	// 
	////////////////////////////
	void Normal::Enter() {

	}

	void Normal::Update(float deltaTime) {
		const auto& input = player_->GetInput();

		Vector3 handPos = player_->GetPosition();
		handPos.x += 1.0f; // プレイヤーの右方向へオフセット
		handPos.y += 1.0f; // 少し上へ

		player_->GetRightHandJoint()->SetPos(handPos);

		// 特になにもしていない時のState
		if (input.isAttack) {
			switch (player_->GetWeaponStance()) {
			case WeaponStance::Sheathed:
				// 納刀状態なら攻撃をする前に抜刀するようにする
				// この時、攻撃をする意思があることを伝えるようにする
				// ChangeState -> DrawWeapon -> Attack
				player_->ChangeActionState(player_->drawWeaponState_.get());
				break;
			case WeaponStance::Drawn:
				// 抜刀状態なら即攻撃のStateに遷移出来る
				// ChangeState -> Attack
				player_->ChangeActionState(player_->attackState_.get());
				break;
				// その他の状態なら攻撃が出来ないのでどうなるか分からない
				// ただ、残りの状態なら同じ処理になるのでBreakせず処理をまとめる
			}
		}
		else if (input.isSheathe) {
			switch (player_->GetWeaponStance()) {
			case WeaponStance::Sheathed:
				// なにもない
				break;
			case WeaponStance::Drawn:
				// 抜刀状態なら納刀のStateに遷移出来る
				// ChangeState -> SheatheWeapon
				player_->ChangeActionState(player_->sheatheWeaponState_.get());
				break;
				// ほかもSheatheでいいかな
			}
		}
	}

	void Normal::Exit() {

	}

	////////////////////////////
	//
	//  Attack
	// 
	////////////////////////////
	void Attack::Enter() {
		//Vector3 startPos = { player_->GetRightHandJoint()->GetMatrix().m[3][0],player_->GetRightHandJoint()->GetMatrix().m[3][1] ,player_->GetRightHandJoint()->GetMatrix().m[3][2] };
		motion_.Play("Swing", {0.0f,0.0f,0.0f}, 0.4f);
	}

	void Attack::Update(float deltaTime) {
		Vector3 handPos = player_->GetPosition();
		handPos.x += 1.0f; // プレイヤーの右方向へオフセット
		handPos.y += 1.0f; // 少し上へ
		player_->GetRightHandJoint()->SetPos(motion_.Update(deltaTime, player_->eyesDirection_) + handPos);

		if (!motion_.IsPlaying()) {
			player_->ChangeActionState(player_->normalState_.get());
		}
	}

	void Attack::Exit() {

	}

	////////////////////////////
	//
	//  Sheathe
	// 
	////////////////////////////
	void SheatheWeapon::Enter() {
		// 納刀のアニメーション再生
		// 予約の初期化
		player_->ConsumeReservedAction();
	}

	void SheatheWeapon::Update(float deltaTime) {
		const auto& input = player_->GetInput();
		if (input.isAttack) {
			// 攻撃の予約を行う
			player_->ReserveActionState(player_->attackState_.get());
		}
		else if (input.isEvasion) {
			// 回避の予約を行う
			//player_->ReserveActionState(player_->evasionState_.get());
		}
		
		if (true/*再生が終わったら*/) {
			// 状態を納刀状態に変化
			player_->SetWeaponStance(WeaponStance::Sheathed);

			// 予約が存在するか確認する
			PlayerStates::Base* nextAction = player_->ConsumeReservedAction();


			if (nextAction != nullptr) {
				// 予約が存在すればそのまま送る
				player_->ChangeActionState(nextAction);
			}
			else {
				// 終わったので通常状態に戻す
			// ※ もしなにか他にあるならこれより前に書く
				player_->ChangeActionState(player_->normalState_.get());
			}
		}
	}

	void SheatheWeapon::Exit() {

	}
	////////////////////////////
	//
	//  Draw
	// 
	////////////////////////////
	void DrawWeapon::Enter() {
		// 抜刀のアニメーションを再生
		// 予約の初期化
		player_->ConsumeReservedAction();
	}

	void DrawWeapon::Update(float deltaTime) {
		const auto& input = player_->GetInput();
		if (input.isAttack) {
			// 攻撃の予約を行う
			player_->ReserveActionState(player_->attackState_.get());
		}

		if (true/*再生が終わったら*/) {
			// 状態を抜刀状態に変化
			player_->SetWeaponStance(WeaponStance::Drawn);

			// 予約が存在するか確認する
			PlayerStates::Base* nextAction = player_->ConsumeReservedAction();

			if (nextAction != nullptr) {
				// 予約が存在すればそのまま送る
				player_->ChangeActionState(nextAction);
			}
			else {
				// 終わったので通常状態に戻す
			// ※ もしなにか他にあるならこれより前に書く
				player_->ChangeActionState(player_->normalState_.get());
			}
		}
	}

	void DrawWeapon::Exit() {

	}
}