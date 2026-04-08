#include "Umbrella.h"
#include "UmbrellaState.h"

namespace UmbrellaStates {
	/////////////////////////
	/// 
	///  Close
	///
	/////////////////////////
	void Close::Enter() {
		// アニメーション開始
	}
	void Close::Update(float deltaTime) {
		if (true/*アニメーションが終わったら*/) {
			top_->ChangeState(new UmbrellaStates::Attached());
		}
	}
	void Close::Exit() {
		top_->ChangeForm(UmbrellaForm::Closed);
	}
	/////////////////////////
	/// 
	///  Open
	///
	/////////////////////////
	void Open::Enter() {
		// アニメーション開始
	}
	void Open::Update(float deltaTime) {
		if (true/*アニメーションが終わったら*/) {
			top_->ChangeState(new UmbrellaStates::Attached());
		}
	}
	void Open::Exit() {
		top_->ChangeForm(UmbrellaForm::Opened);
	}
	/////////////////////////
	/// 
	///  Reverse
	///
	/////////////////////////
	void Reverse::Enter() {
		// アニメーション開始
	}
	void Reverse::Update(float deltaTime) {
		if (true/*アニメーションが終わったら*/) {
			top_->ChangeState(new Attached());
		}
	}
	void Reverse::Exit() {
		// 状態の設定
		top_->ChangeForm(UmbrellaForm::Reverse);
		// 防御力を低く設定
		top_->GetStatusComponent().SetDefense(1.0f);
	}
	/////////////////////////
	/// 
	///  Broken
	///
	/////////////////////////
	void Broken::Enter() {
		// アニメーション開始
		
	}
	void Broken::Update(float deltaTime) {
		
	}
	void Broken::Exit() {
		
	}
	/////////////////////////
	/// 
	///  Attached
	///
	/////////////////////////
	void Attached::Enter() {

	}
	void Attached::Update(float deltaTime) {
		
	}
	void Attached::Exit() {

	}

	/////////////////////////
	/// 
	///  NormalAttack
	///
	/////////////////////////
	void NormalAttack::Enter() {
		// 傘の当たり判定をON
		top_->EnableAttackCollision();
	}

	void NormalAttack::Update(float deltaTime) {
		
		//// モーション（振り）が終わったら、自動的に「いつもの手持ち状態」に戻る！
		//if (!motion_.IsPlaying()) {
		//	top_->ChangeState(new UmbrellaStates::Attached());
		//}
	}

	void NormalAttack::Exit() {
		// 状態が終わる時（Attachedに戻る瞬間）に、当たり判定をOFFにする
		top_->DisableAttackCollision();
	}

	/////////////////////////
	/// 
	///  Flying
	///
	/////////////////////////
	void Flying::Enter() {
		// 親（持ち手）から自分（かさ）を切り離す！
		// ※ Topクラスが持っているJoint（アタッチメント）をDetachする処理
		top_->GetRootJoint()->Detach();

		top_->ChangeForm(UmbrellaForm::Flying); // 飛んでいる間は「かさがない状態」にする
	}

	void Flying::Update(float deltaTime) {
		// ③ 座標を更新して飛ばす
		Vector3 pos = top_->GetRootJoint()->GetPos();
		pos.x += velocity_.x * deltaTime;
		pos.y += velocity_.y * deltaTime;
		pos.z += velocity_.z * deltaTime;
		top_->GetRootJoint()->SetPos(pos);
		// Velocityをだんだん減速させる
		float deceleration = 15.0f; // ブレーキの強さ
		velocity_.x = std::lerp(velocity_.x, 0.0f, deceleration * deltaTime);
		velocity_.y = std::lerp(velocity_.z, 0.0f, deceleration * deltaTime);

		// 速度がゼロになったら飛ぶのを終了する
		if (velocity_.x <= 0.0001f && velocity_.y <= 0.0001f) {
			top_->ChangeState(new Stationary());
		}
	}

	void Flying::Exit() {
		// 飛び終わった時の処理
	}

	// ==========================================
	//   Stationary (静止・足場状態)
	// ==========================================
	void Stationary::Enter() {
		// 足場用の当たり判定（コライダー属性）をONにするなどの処理
		top_->GetCollider()->SetMyType(COL_Umbrella_Ground);
		top_->UpdateColliderShape();
	}

	void Stationary::Update(float deltaTime) {
		// ここに留まり続ける。
		// もしプレイヤーが「回収ボタン」を押したら、手元に戻るステートへ移行など

	}

	void Stationary::Exit() {
		// 足場判定をOFFにするなど
		top_->GetCollider()->SetMyType(COL_None);
	}
}