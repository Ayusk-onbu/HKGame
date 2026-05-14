#include "States.h"
#include "Character.h"
#include "MathUtils.h"

namespace CharacterState {
	namespace Movement {
		///////////////////////////////
		/// 
		///   Ground Base (地上にいるときの処理)
		/// 
		///////////////////////////////
		void GroundBase::Enter() {

		}
		void GroundBase::Update(float deltaTime) {

			if (character_->OnGround()) {
				if (character_->GetCommand().subMove == ButtonState::Pressed) {
					character_->Jump();
					return;
				}
			}
			// 地面から離れていたらの処理
			else {
				// 落っこちたのならコヨーテタイム開始
				character_->ResetCoyoteTime();
				
				character_->ChangeMovementState("AirBase");
				return;
			}
		}
		void GroundBase::Exit() {

		}

		///////////////////////////////
		/// 
		///   Air Base (空中にいるときの処理)
		/// 
		///////////////////////////////
		void AirBase::Enter() {

		}
		void AirBase::Update(float deltaTime) {
			if (character_->OnGround()) {
				// 着地したらの処理
				character_->ChangeMovementState("Idle");
			}
			else {
				// 地面から離れていたらの処理
				if (character_->GetCoyoteTimer() > 0.0f) {
					character_->DecreaseCoyoteTimer(deltaTime);

					if (character_->GetCommand().subMove == ButtonState::Pressed) {
						character_->Jump();
						return;
					}
				}
			}
		}
		void AirBase::Exit() {

		}

		///////////////////////////////
		/// 
		///   Restricted Base (移動制限状態の処理)
		/// 
		///////////////////////////////
		void Restricted::Enter() {

		}
		void Restricted::Update(float deltaTime) {

		}
		void Restricted::Exit() {

		}

		///////////////////////////////
		/// 
		///   Idle State（特に何もしていないときの処理）
		/// 
		///////////////////////////////
		void Idle::Enter() {

		}
		void Idle::Update(float deltaTime) {
			if (parentState_) {
				parentState_->Update(deltaTime);
			}

			Vector3 moveDir = character_->GetCommand().moveDirection; // コマンドから移動入力を取得

			if (Length(moveDir) > 0.0f) {
				character_->ChangeMovementState("Walk"); // 移動入力があればWalk状態に変更
				return;
			}

			// 地面の摩擦を取得
			float friction = character_->GetCurrentGroundFriction();

			// 摩擦を適用（氷なら 0.98f などになりよく滑り、泥なら 0.2f などですぐ止まる）
			character_->SetMyVelocity(character_->GetMyVelocity() * friction);
		}
		void Idle::Exit() {

		}

		///////////////////////////////
		/// 
		///   Walk State（移動中の処理）
		/// 
		///////////////////////////////
		void Walk::Enter() {

		}
		void Walk::Update(float deltaTime) {
			if (parentState_) {
				parentState_->Update(deltaTime);
			}
			Vector3 moveDir = character_->GetCommand().moveDirection; // コマンドから移動入力を取得

			if (Length(moveDir) == 0.0f) {
				character_->ChangeMovementState("Idle"); // 移動入力がなければIdle状態に変更
				return;
			}

			// 移動方向と視線の方向の「一致度」を計算 (-1.0 ～ 1.0)
			// 1.0 なら同じ向き、0.0 なら真横、-1.0 なら真後ろ
			float similarity = Dot(Normalize(moveDir), Normalize(character_->GetEyesDirection()));

			// 2. 一致度に基づいて速度係数を決める
			float speedMultiplier = 1.0f;
			if (similarity < 0.8f) { // ちょっとでも横や後ろを向いていたら
				// 例: 真後ろ(-1.0)のときは 0.5倍速、正面(1.0)のときは 1.0倍速 になるように線形補間
				speedMultiplier = Lerp(0.5f, 1.0f, (similarity + 1.0f) * 0.5f);
			}

			moveDir = moveDir.z * CameraSystem::GetInstance()->GetActiveCamera()->zAxis_ + moveDir.x * CameraSystem::GetInstance()->GetActiveCamera()->xAxis_;
			moveDir.y = 0.0f;
			moveDir = Normalize(moveDir);

			character_->SetMyVelocity(moveDir * (character_->GetStatus().GetCurrentSpeed() * speedMultiplier)); // 移動入力に基づいて速度を設定
		}
		void Walk::Exit() {

		}
	}
}