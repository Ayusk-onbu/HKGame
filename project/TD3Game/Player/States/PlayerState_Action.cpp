#include "../PlayerStates.h"
#include "../Player.h"
#include "ImGuiManager.h"

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
		const auto& umbrella = player_->GetUmbrella();

		Vector3 handPos = player_->GetPosition();
		handPos.x += 1.0f * player_->eyesDirection_.x; // プレイヤーの右方向へオフセット
		handPos.y += 1.0f; // 少し上へ

		player_->GetRightHandJoint()->SetPos(handPos);

		if (true/*player_->GetUmbrella().top_->GetUmbrellaForm() != UmbrellaForm::Flying*/) {
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
					if (umbrella.top_->GetUmbrellaForm() == UmbrellaForm::Closed) {
						player_->ChangeActionState(player_->attackState_.get());
						return;
					}
					if (input.isAttackHeld) {
						if (umbrella.top_->GetUmbrellaForm() == UmbrellaForm::Reverse) {
							player_->ChangeActionState(player_->reverseChargeState_.get());
							return;
						}
					}
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
					if (umbrella.top_->GetUmbrellaForm() == UmbrellaForm::Closed) {
						player_->ChangeActionState(player_->sheatheWeaponState_.get());
					}
					else {
						player_->ChangeActionState(player_->umbrellaCloseState_.get());
					}
					break;
					// ほかもSheatheでいいかな
				}
			}
			else if (input.isGuard) {
				switch (player_->GetWeaponStance()) {
				case WeaponStance::Sheathed:
					// 抜刀状態にしてガードする？一旦わからない
					break;
				case WeaponStance::Drawn:
					// 抜刀状態なので傘は開いているか確認する
					if ((umbrella.top_->GetUmbrellaForm() == UmbrellaForm::Opened) || (umbrella.top_->GetUmbrellaForm() == UmbrellaForm::Reverse)) {
						player_->ChangeActionState(player_->guardState_.get());
					}
					else {
						// 傘を開くStateに遷移する。-> ガードを押していたらガードStateに遷移する。
						player_->ChangeActionState(player_->umbrellaOpenState_.get());
					}
					break;
				}
			}
			else if (input.isReverse) {
				switch (player_->GetWeaponStance()) {
				case WeaponStance::Sheathed:
					// 抜刀状態にしてガードする？一旦わからない
					break;
				case WeaponStance::Drawn:
					// 抜刀状態なので傘は開いているか確認する
					if ((umbrella.top_->GetUmbrellaForm() == UmbrellaForm::Opened)) {
						player_->ChangeActionState(player_->umbrellaReverseState_.get());
					}
					else {
						// 傘を開くStateに遷移する。-> ガードを押していたらガードStateに遷移する。
						//player_->ChangeActionState(player_->umbrellaOpenState_.get());
					}
					break;
				}
			}
			else if (input.isRepair) {
				// 耐久値が減っている時（MAXじゃない時）だけ修理できるようにする
				if (umbrella.top_->GetStatusComponent().GetHp() < umbrella.top_->GetStatusComponent().GetMaxHp()) {
					player_->ChangeActionState(player_->repairUmbrellaState_.get());
					return;
				}
			}
		}
		else {
			
		}
		if (input.isShoot) {
			if (umbrella.top_->GetUmbrellaForm() == UmbrellaForm::Flying) {
				player_->WarpToUmbrella();
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
		attackTimer_ = 0.0f;
		isNextAttackReserved_ = false;

		// 移動ステートを「入力を受け付けない状態(Restricted)」に強制変更
		player_->ChangeMovementState(player_->restrictedState_.get());

		// 進む方向の設定
		Vector3 forward = player_->eyesDirection_;// プレイヤーが向いている方向に決定(順次変更点)

		// ========================================================
		// 【 コンボ段数に応じた踏み込みの強さとモーションの設定 】
		// ========================================================
		float stepPower = 0.0f;
		if (comboCount_ == 1) { 
			stepPower = 4.0f;
			motion_.Play("SwingFirst", { 0.0f,0.0f,0.0f }, 0.5f);
		}
		else if (comboCount_ == 2) { 
			stepPower = 6.0f;
			motion_.Play("SwingSecond", { 0.0f,0.0f,0.0f }, 0.4f);
		}
		else if (comboCount_ == 3) { 
			stepPower = 30.0f;
			if (player_->GetInput().useMana) {
				if (player_->GetManaComponent().HasEnoughMana(30.0f)) {
					player_->GetManaComponent().ConsumeMana(30.0f);
					motion_.Play("SwingMana", { 0.0f,0.0f,0.0f }, 0.4f);
					stepPower = 55.0f;
				}
				else {
					motion_.Play("SwingLast", { 0.0f,0.0f,0.0f }, 0.3f);
				}
			}
			else {
				motion_.Play("SwingLast", { 0.0f,0.0f,0.0f }, 0.3f);
			}
		}

		// 踏み込みの初速を与える（これがRestrictedステート内で徐々に減速していく）
		player_->myVelocity_.x = forward.x * stepPower;
		player_->myVelocity_.z = forward.z * stepPower;

		// =============
		// 【 傘の設定 】
		// =============
		player_->GetUmbrella().top_->ChangeState(new UmbrellaStates::NormalAttack());
	}

	void Attack::Update(float deltaTime) {
		attackTimer_ += deltaTime;
		const auto& input = player_->GetInput();

		// =================================
		// 【 手のJoint位置の設定 】
		// =================================
		Vector3 handPos = player_->GetPosition();
		handPos.x += 1.0f * player_->eyesDirection_.x; // プレイヤーの右方向へオフセット
		handPos.y += 1.0f; // 少し上へ
		player_->GetRightHandJoint()->SetPos(motion_.Update(deltaTime, player_->eyesDirection_) + handPos);

		/*if (!motion_.IsPlaying()) {
			player_->ChangeActionState(player_->normalState_.get());
		}*/

		// =================================
		// 【 先行入力 】
		// =================================
		if (input.isAttack && attackTimer_ > 0.1f) {
			isNextAttackReserved_ = true;
		}

		float currentMotionDuration = motion_.GetMotionDuration();

		// =================================
		// 【 次のモーションへの派生、または終了 】
		// =================================
		if (attackTimer_ >= currentMotionDuration) {
			if (isNextAttackReserved_ && comboCount_ < 3) {
				comboCount_++;
				Enter(); // 次の段へ
			}
			else {
				// コンボ終了
				comboCount_ = 1;
				player_->ChangeActionState(player_->normalState_.get());

				// 攻撃が終わったので、移動ステートを元に戻す（Idleにして入力を再開させる）
				player_->ChangeMovementState(player_->idleState_.get());

				// 傘の設定を戻す
				player_->GetUmbrella().top_->ChangeState(new UmbrellaStates::Attached());
			}
		}
	}

	void Attack::Exit() {
		// =================================
		// 【 別のステート（回避や被ダメージなど）で強制終了させられた時のためのリセット 】
		// =================================
		comboCount_ = 1;
		isNextAttackReserved_ = false;
		// 攻撃判定(Collider)をオフにする処理などもここに書く
	}
	////////////////////////////
	// 
	// ThrowUmbrella 
	// 
	////////////////////////////
	void ThrowUmbrella::Enter() {
		// アニメーション再生
	}

	void ThrowUmbrella::Update(float deltaTime) {
		// プレイヤーの手の位置に傘を追従させる
		Vector3 handPos = player_->GetPosition();
		handPos.x += 1.0f * player_->eyesDirection_.x; // プレイヤーの右方向へオフセット
		handPos.y += 1.0f; // 少し上へ
		player_->GetRightHandJoint()->SetPos(handPos);
		
		if (true/*再生が終わったら*/) {
			Vector3 throwVelocity = player_->GetTargetPos() - player_->GetPosition();
			float throwSpeed = 25.0f; // 投げる速度の調整用の定数
			throwVelocity.x *= throwSpeed;
			throwVelocity.y *= throwSpeed;
			player_->GetUmbrella().top_->ChangeState(new UmbrellaStates::Flying(throwVelocity));
			player_->ChangeActionState(player_->normalState_.get());
		}
		
	}

	void ThrowUmbrella::Exit() {

	}

	////////////////////////////
	// 
	// ReverseChargeAttack 
	// 
	////////////////////////////
	void ReverseCharge::Enter() {

	}

	void ReverseCharge::Update(float deltaTime) {
		// =================================
		// 【 手のJoint位置の設定 】
		// =================================
		Vector3 handPos = player_->GetPosition();
		handPos.x += 1.0f * player_->eyesDirection_.x; // プレイヤーの右方向へオフセット
		handPos.y += 1.0f; // 少し上へ
		player_->GetRightHandJoint()->SetPos(handPos);

		// 1秒間に溜まるマナの量
		float chargeSpeed = 10.0f * deltaTime;

		// プレイヤーのマナコンポーネントを取得
		ManaComponent& mana = player_->GetManaComponent();

		// マナが満タンじゃなければ、通常のマナを回復！
		if (mana.GetCurrentMana() < mana.GetMaxMana()) {
			mana.AddMana(chargeSpeed); // ※ManaComponentの回復関数を呼ぶ
		}
		// 満タンなら、傘に「過剰マナ（水）」を溜める！
		else {
			player_->GetUmbrella().top_->GetManaComponent().AddMana(chargeSpeed);

			// ※ここで傘のモデルを少し膨張させたり、水のエフェクトを濃くする
		}

		// 攻撃ボタンを離したら、チャージ攻撃ステートへ移行！
		if (player_->GetInput().isAttackReleased) {
			player_->ChangeActionState(player_->reverseAttackState_.get());
		}

		ImGui::Text("Charging... Current Mana: %.1f / %.1f, Umbrella Mana: %.1f / %.1f", mana.GetCurrentMana(), mana.GetMaxMana(), player_->GetUmbrella().top_->GetManaComponent().GetCurrentMana(), player_->GetUmbrella().top_->GetManaComponent().GetMaxMana());
	}

	void ReverseCharge::Exit() {

	}
	////////////////////////////
	//
	//  ReverseAttack
	// 
	////////////////////////////
	void ReverseAttack::Enter() {
		// 傘に溜まった過剰マナをすべて取り出す！
		float extraDamage = player_->GetUmbrella().top_->GetManaComponent().GetCurrentMana();

		// 基礎攻撃力 ＋ 過剰マナによるボーナスダメージ！
		float baseAttack = 0.0f; // 通常の逆さ攻撃の威力
		float finalAttack = baseAttack + extraDamage;
		player_->GetUmbrella().top_->GetManaComponent().ConsumeMana(extraDamage); // 傘のマナを0にする

		// 傘の StatusComponent の攻撃力を一時的に書き換える
		player_->GetUmbrella().top_->GetStatusComponent().SetAttack(finalAttack);

		// 傘自体を「攻撃ステート」にして当たり判定をONにする
		player_->GetUmbrella().top_->ChangeState(new UmbrellaStates::NormalAttack());

		// ※ プレイヤーの攻撃モーション（バシャーン！と水をぶちまける）を再生
		motion_.Play("Swing", { 0.0f,0.0f,0.0f }, 0.5f);
	}

	void ReverseAttack::Update(float deltaTime) {
		Vector3 handPos = player_->GetPosition();
		handPos.x += 1.0f * player_->eyesDirection_.x; // プレイヤーの右方向へオフセット
		handPos.y += 1.0f; // 少し上へ
		player_->GetRightHandJoint()->SetPos(motion_.Update(deltaTime, player_->eyesDirection_) + handPos);

		// 攻撃モーションが終わったら、通常の攻撃状態に戻す
		if (motion_.IsPlaying() == false) {
			// 傘の攻撃ステートを解除して通常状態に戻す
			player_->GetUmbrella().top_->ChangeState(new UmbrellaStates::Attached());
			// プレイヤーの攻撃モーションも終了させる
			player_->ChangeActionState(player_->normalState_.get());
		}
	}

	void ReverseAttack::Exit() {
		// 傘の攻撃力を元に戻す
		player_->GetUmbrella().top_->GetStatusComponent().SetAttack(10.0f); // 基礎攻撃力にリセット
	}

	////////////////////////////
	//
	//  Guard
	// 
	////////////////////////////
	void Guard::Enter() {
		// プレイヤーのガードのアニメーションを開始

		// 座標の設定をする

	}

	void Guard::Update(float deltaTime) {
		const auto& input = player_->GetInput();

		Vector3 handPos = player_->GetPosition();
		handPos.x += 1.0f * player_->eyesDirection_.x; // プレイヤーの右方向へオフセット
		handPos.y += 1.0f; // 少し上へ

		float rotAmount = 50.0f;
		Vector3 handRot = {0.0f,0.0f, Deg2Rad(-(rotAmount * input.moveDirection.x))};

		player_->GetRightHandJoint()->SetPos(handPos);
		player_->GetRightHandJoint()->SetRot(handRot);


		if (input.isGuard == false) {
			// ガードボタンを離したら終わる
			player_->ChangeActionState(player_->normalState_.get());
		}
	}

	void Guard::Exit() {
		// ここでなにかするかも
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
			//player_->ReserveActionState(player_->attackState_.get());
		}
		else if (input.isEvasion) {
			// 回避の予約を行う
			//player_->ReserveActionState(player_->evasionState_.get());
		}
		
		if (true/*再生が終わったら*/) {
			// 状態を納刀状態に変化
			player_->SetWeaponStance(WeaponStance::Sheathed);

			// 「傘」のJointを背中に移す
			player_->UmbrellaAttachBack();

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

			// UmbrellaのJointを背中に移す
			player_->UmbrellaAttachRHand();

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
	////////////////////////////
	//
	//  Umbrella Open
	// 
	////////////////////////////
	void UmbrellaOpen::Enter() {
		// 傘を開くプレイヤーのアニメーションを開始

		// 傘に開くよう命令を下す
		player_->GetUmbrella().top_->ChangeState(new UmbrellaStates::Open());
		// 予約の初期化
		player_->ConsumeReservedAction();
	}
	void UmbrellaOpen::Update(float deltaTime) {
		const auto& umbrella = player_->GetUmbrella();
		const auto& input = player_->GetInput();
		Vector3 handPos = player_->GetPosition();
		handPos.x += 1.0f * player_->eyesDirection_.x; // プレイヤーの右方向へオフセット
		handPos.y += 1.0f; // 少し上へ

		player_->GetRightHandJoint()->SetPos(handPos);

		if (input.isGuard) {
			// 行動の予約を行う
			player_->ReserveActionState(player_->guardState_.get());
		}

		// 傘側のStateで、開き終わったら型が変わる
		// -> 変わったらこちらのStateも別のステートに変更する
		if (umbrella.top_->GetUmbrellaForm() == UmbrellaForm::Opened) {
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
	void UmbrellaOpen::Exit() {
		// 特になし

	}
	////////////////////////////
	//
	//  Umbrella Close
	// 
	////////////////////////////
	void UmbrellaClose::Enter() {
		// 傘を開くプレイヤーのアニメーションを開始

		// 傘に開くよう命令を下す
		player_->GetUmbrella().top_->ChangeState(new UmbrellaStates::Close());
		// 予約の初期化
		player_->ConsumeReservedAction();
	}
	void UmbrellaClose::Update(float deltaTime) {
		const auto& umbrella = player_->GetUmbrella();
		const auto& input = player_->GetInput();
		Vector3 handPos = player_->GetPosition();
		handPos.x += 1.0f * player_->eyesDirection_.x; // プレイヤーの右方向へオフセット
		handPos.y += 1.0f; // 少し上へ

		player_->GetRightHandJoint()->SetPos(handPos);

		if (input.isSheathe) {
			// 行動の予約を行う
			player_->ReserveActionState(player_->sheatheWeaponState_.get());
		}

		// 傘側のStateで、開き終わったら型が変わる
		// -> 変わったらこちらのStateも別のステートに変更する
		if (umbrella.top_->GetUmbrellaForm() == UmbrellaForm::Closed) {
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
	void UmbrellaClose::Exit() {

	}
	////////////////////////////
	//
	//  Umbrella Reverse
	// 
	////////////////////////////
	void UmbrellaReverse::Enter() {
		// 傘を開くプレイヤーのアニメーションを開始

		// 傘に開くよう命令を下す
		player_->GetUmbrella().top_->ChangeState(new UmbrellaStates::Reverse());
		// 予約の初期化
		player_->ConsumeReservedAction();

	}
	void UmbrellaReverse::Update(float deltaTime) {
		const auto& umbrella = player_->GetUmbrella();
		const auto& input = player_->GetInput();
		Vector3 handPos = player_->GetPosition();
		handPos.x += 1.0f * player_->eyesDirection_.x; // プレイヤーの右方向へオフセット
		handPos.y += 1.0f; // 少し上へ

		player_->GetRightHandJoint()->SetPos(handPos);

		//if (input.isSheathe) {
		//	// 行動の予約を行う
		//	player_->ReserveActionState(player_->sheatheWeaponState_.get());
		//}

		// 傘側のStateで、開き終わったら型が変わる
		// -> 変わったらこちらのStateも別のステートに変更する
		if (umbrella.top_->GetUmbrellaForm() == UmbrellaForm::Reverse) {
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
	void UmbrellaReverse::Exit() {

	}
	////////////////////////////
	//
	//  Repair Umbrella
	// 
	////////////////////////////
	void RepairUmbrella::Enter() {
		// パラメータの初期化
		repairTimer_ = 0.0f;
		finishTimer_ = 0.0f;
		isFinishing_ = false;

		// 傘を修理するアニメーションを開始

		// 予約の初期化
		player_->ConsumeReservedAction();
	}

	void RepairUmbrella::Update(float deltaTime) {
		// ===================
		// 【 手の位置の調整 】
		// ===================
		Vector3 handPos = player_->GetPosition();
		handPos.x += 1.0f * player_->eyesDirection_.x; // プレイヤーの右方向へオフセット
		handPos.y += 1.0f; // 少し上へ
		player_->GetRightHandJoint()->SetPos(handPos);

		const auto& input = player_->GetInput();
		auto& status = player_->GetUmbrella().top_->GetStatusComponent();
		//   =====================================================
		// 【 回避（コロリン）によるキャンセル処理（いつでも可能）】
		//   =====================================================
		if (input.isEvasion) {
			// ※ここでエフェクトや音を止める処理を入れる
			// 回避ステートへ強制移行して修理を中断！
			// player_->ChangeActionState(player_->evasionState_.get());
			return;
		}

		//   ===========================
		// 【 0.5秒ごとのループ回復処理 】
		//   ===========================
		repairTimer_ += deltaTime;

		if (repairTimer_ >= REPAIR_INTERVAL) {
			// 最大HPの20%を計算して回復
			float healAmount = HEAL_PERCENTAGE;
			status.Heal(healAmount);

			// ※ここで「カシャッ」という短い修理SEや、火花エフェクトを出す

			// 全回復したかチェック
			if (status.GetHp() >= status.GetMaxHp()) {
				// 全回復した！完了モーションへ移行
				isFinishing_ = true;

				// ※ここで「完成アニメーション（立ち上がる等）」の再生
				// 「シャキーン！」という完了SEを鳴らす
			}
			else {
				// まだ回復できるならタイマーをリセットしてループ
				repairTimer_ = 0.0f;

				// ※ここで修理アニメーションを最初から再生し直す（ループさせる）
			}
		}

		// --------------------------------------------------------
		// 完了モーション中の処理
		// --------------------------------------------------------
		if (isFinishing_) {
			finishTimer_ += deltaTime;
			if (finishTimer_ >= FINISH_TIME) {
				// 完了モーションが終わったら通常状態へ戻る
				player_->ChangeActionState(player_->normalState_.get());
			}
			return; // 完了モーション中は回復ループの処理をしない
		}

		
	}

	void RepairUmbrella::Exit() {

	}
}