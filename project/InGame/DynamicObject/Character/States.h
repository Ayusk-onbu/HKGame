#pragma once

class Character;

namespace CharacterState {
	class Base {
	public:
		virtual ~Base() = default;
	public:
		virtual void Enter() {}
		virtual void Update(float deltaTime) {}
		virtual void Exit() {}
	public:
		void SetInfo(Character* character, Base* parent = nullptr) { character_ = character; parentState_ = parent; }
		void SetParentState(Base* parent) { parentState_ = parent; }
	protected:
		Character* character_ = nullptr;
		Base* parentState_ = nullptr;
	};

	namespace Movement {
		// =============
		// 【 親State 】
		// =============
		// 地上にいるときの共通処理
		class GroundBase : public Base {
		public:
			~GroundBase()override = default;
		public:
			void Enter() override;
			void Update(float deltaTime) override;
			void Exit() override;

		};
		// 空中にいるときの共通処理
		class AirBase : public Base {
		public:
			~AirBase()override = default;
		public:
			void Enter() override;
			void Update(float deltaTime) override;
			void Exit() override;
		};
		// 移動制限状態（例: スタン、拘束、特殊なアクションなど）
		class Restricted : public Base {
		public:
			~Restricted()override = default;
		public:
			void Enter() override;
			void Update(float deltaTime) override;
			void Exit() override;
		};

		// ===================
		// 【 子State：Ground 】
		// ===================

		class Idle : public Base {
		public:
			~Idle()override = default;
		public:
			void Enter() override;
			void Update(float deltaTime) override;
			void Exit() override;
		};

		class Walk : public Base {
		public:
			~Walk()override = default;
		public:
			void Enter() override;
			void Update(float deltaTime) override;
			void Exit() override;
		};
	}
}