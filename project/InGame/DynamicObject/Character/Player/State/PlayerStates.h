#pragma once
#include "../../States.h"

class Player;

using namespace CharacterState;

namespace PlayerStates {

	namespace Action {
		class Idle : public Base {
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
		};
	}

	namespace Magic {

	}
}
