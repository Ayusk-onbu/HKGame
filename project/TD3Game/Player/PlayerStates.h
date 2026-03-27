#pragma once
#include "MotionManager.h"
class Player;

namespace PlayerStates {
	class Base {
	public:
		virtual ~Base() = default;
		virtual void Enter() = 0;
		virtual void Exit() = 0;
		virtual void SetInfo(Player* player, Base* parent = nullptr) { player_ = player; parentState_ = parent; }
		virtual void Update(float deltaTime) {
			if (parentState_) {
				parentState_->Update(deltaTime);
			}
		}
	protected:
		Player* player_;
		Base* parentState_;
	};
	namespace Movement {
		// 親ステート
		class Grounded : public Base {
		public:
			void Enter() override {}
			void Exit() override {}
			void Update(float deltaTime) override;
		};

		class Airborne : public Base {
		public:
			void Enter() override {}
			void Exit() override {}
			void Update(float deltaTime) override;
		};

		// 子ステート
		class Idle : public Base {
		public:
			void Enter() override;
			void Update(float deltaTime) override;
			void Exit() override;
		};
		class Walking : public Base {
		public:
			void Enter() override;
			void Update(float deltaTime) override;
			void Exit() override;
		};
		class Running : public Base {
		public:
			void Enter() override {};
			void Update(float deltaTime) override {};
			void Exit() override {};
		};
		// アクション実行中などで移動入力を受け付けないステート
		class Restricted : public Base {
		public:
			void Enter() override;
			void Update(float deltaTime) override;
			void Exit() override;
		};
	}
	namespace Action {
		class Normal : public Base {
		public:
			void Enter() override;
			void Update(float deltaTime) override;
			void Exit() override;
		};
		class Attack : public Base {
		public:
			void Enter() override;
			void Update(float deltaTime) override;
			void Exit() override;
		private:
			MotionController motion_;
			int comboCount_ = 1;
			float attackTimer_ = 0.0f;
			bool isNextAttackReserved_ = false;
		};
		class Guard : public Base {
		public:
			void Enter() override;
			void Update(float deltaTime) override;
			void Exit() override;
		private:
			MotionController motion_;
		};
		class Dash : public Base {
		public:
			void Enter() override;
			void Update(float deltaTime) override;
			void Exit() override;
		};
		class Evasion : public Base {
		public:
			void Enter() override;
			void Update(float deltaTime) override;
			void Exit() override;
		};
		class DrawWeapon : public Base {
		public:
			void Enter()override;
			void Update(float deltaTime)override;
			void Exit()override;
		};
		class SheatheWeapon : public Base {
		public:
			void Enter()override;
			void Update(float deltaTime)override;
			void Exit()override;
		};

		class UmbrellaOpen : public Base {
		public:
			void Enter()override;
			void Update(float deltaTime)override;
			void Exit()override;
		};
		class UmbrellaClose : public Base {
		public:
			void Enter()override;
			void Update(float deltaTime)override;
			void Exit()override;
		};
		class UmbrellaReverse : public Base {
		public:
			void Enter()override;
			void Update(float deltaTime)override;
			void Exit()override;
		};
	}
}