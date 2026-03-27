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
		top_->ChangeForm(UmbrellaForm::Reverse);
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

	//////////////////////////
	///
	///   Thrown
	///
	//////////////////////////
	void Thrown::Enter() {
		Vector3 startPos = { top_->GetRootJoint()->GetMatrix().m[3][0],top_->GetRootJoint()->GetMatrix().m[3][1] ,top_->GetRootJoint()->GetMatrix().m[3][2] };
		motion_.Play("", startPos, 1.0f);
	}
	void Thrown::Update(float deltaTime) {

	}
	void Thrown::Exit() {

	}
}