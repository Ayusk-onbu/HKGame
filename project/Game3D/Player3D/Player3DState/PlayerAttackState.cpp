#include "PlayerState.h"
#include "Player3D.h"

static const AttackData ComboTable[] = {
	// 0段目: 弱
	{ "Attack_1", 0.1f, 0.2f, 0.4f, 0.15f, 0.35f, 10.0f, 1, AttackTrajectory::VerticalSrash, 0.6f },
	// 1段目: 中パンチ（0段目から繋がる）
	{ "Attack_2", 0.12f, 0.2f, 0.45f, 0.15f, 0.4f, 13.0f, 2, AttackTrajectory::HorizontalSwing, 1.2f },
	// 2段目: 強パンチ（1段目から繋がる。次は無し）
	{ "Attack_3", 0.2f, 0.3f, 0.6f, 0.0f, 0.0f, 15.0f, -1, AttackTrajectory::SpinAttack, 2.5f }
	// 
};

void PlayerAttackState::Initialize() {
	if (!player_) return;

	// スタミナ消費と回復ブロック
	// 攻撃にもスタミナコストがある場合や、回復を止める場合
	player_->BlockStaminaRecovery();
	player_->SetIsViewAttack(true);
	player_->ClearAttackBuffer();
	// 移動をブロック
	player_->BlockMove();

	// タイマーリセット
	attackTimer_ = 0.0f;
}

void PlayerAttackState::Update() {
	// 先行入力のチェック
	// コマンド経由でOnAttackInputが呼ばれるとAttackBufferがTrue

	// 使う攻撃データを獲得
	const AttackData& data = ComboTable[currentComboIndex_];
	// 時間の更新
	const float deltaTime = 1.0f / 60.0f;
	attackTimer_ += deltaTime;

	// 攻撃判定の位置を計算
	Vector3 playerPos = player_->GetPosition();
	Vector3 attackOffset = {};// プレイヤーからの相対位置
	float colliderRadius = 0.0f;
	Vector3 hitBoxWorldPos{};
	// ここで位置計算
	if (attackTimer_ >= data.startup && attackTimer_ <= (data.startup + data.active)) {
		// 攻撃をブロック
		player_->BlockAttack();
		// 0.0f ~ 1.0fに正規化
		float slashTime = (attackTimer_ - data.startup) / data.active;
		slashTime = std::clamp(slashTime, 0.0f, 1.0f);

		// 踏み込ませたりしても良い

		attackOffset = CalculateAttackOffset(data, slashTime);
		colliderRadius = data.range;
		//player_->SetAttackColliderRadius(colliderRadius);
		hitBoxWorldPos = playerPos + (Quaternion::RotateVector(attackOffset, player_->GetPlayerQuaternion()) * 2.0f);
		player_->EnableHitBox(true, hitBoxWorldPos);
	}
	else {
		hitBoxWorldPos = playerPos + (attackOffset * player_->GetForwardVector());
		player_->EnableHitBox(false, hitBoxWorldPos);
	}

	// コンボ遷移処理
	// [ コンボ出来るか時間かどうかを判定 ]
	if (attackTimer_ >= data.comboStart && attackTimer_ <= data.comboEnd) {
		// コンボ受付中に攻撃入力があれば次のコンボに遷移
		if (player_->HasAttackBuffer()) {
			// かつ次にコンボ攻撃があるのかを判定する
			if (data.nextComboIndex != -1) {
				// 攻撃入力をクリア
				player_->ClearAttackBuffer();
				// 次の攻撃（コンボ）状態へ遷移
				player_->ChangeState(new PlayerAttackState(data.nextComboIndex));
				return;
			}
		}
	}

	// 状態遷移ロジック (終了判定)
	if (attackTimer_ >= data.recovery) {
		if (player_->GetVerticalVelocity() <= 0.0f) {
			// 下がり始めたら空中にいる状態にする
			player_->ChangeState(new PlayerFallState());
			return;
		}
		
		if (player_->CanDash()) {
			player_->ClearDashBuffer();
			player_->ChangeState(new PlayerDashState());
			return;
		}

		// 攻撃が終了したら、元の状態に戻る
		player_->ChangeState(new PlayerStopState());
		return;
	}


	//if (!player_->IsOnGround()) {
	//	// 空中判定（崖から落ちた場合など）
	//	player_->ChangeState(new PlayerFallState());
	//	return;
	//}

	ImGuiManager::GetInstance()->Text("AttackState");
	ImGuiManager::GetInstance()->Text(data.name);
}

void PlayerAttackState::Exit()
{
	if (!player_) return;

	// スタミナ回復ブロックの解除
	player_->UnblockStaminaRecovery();

	player_->UnBlockAttack();

	player_->ClearAttackBuffer();
	// 移動を解除
	player_->UnBlockMove();

	player_->SetIsViewAttack(false);

	// 攻撃判定を確実に無効化
	player_->EnableHitBox(false, player_->GetPosition());

	// 他に何かあれば

}

Vector3 PlayerAttackState::CalculateAttackOffset(const AttackData& data, float slashTime) {
	// 攻撃に応じたオフセットを計算
	Vector3 offset = { 0.0f,0.0f,0.0f };
	switch (data.trajectory) {
	case AttackTrajectory::VerticalSrash:
		// 縦切りの軌道計算
		offset.y = 1.8f - (slashTime * 0.5f);
		offset.z = 0.8f;
		break;

	case AttackTrajectory::HorizontalSwing:
		// 横切りの軌道計算
		offset.x = std::sinf(slashTime * 3.14f) * 2.0f;
		offset.y = 1.2f;
		offset.z = 1.0f;
		break;

	case AttackTrajectory::SpinAttack:
		// 周囲を薙ぎ払い
		offset.x = std::cosf(slashTime * 6.28f) * 2.0f;
		offset.z = std::sinf(slashTime * 6.28f) * 2.0f;
		offset.y = 1.0f;
		break;
	}
	return offset;
}