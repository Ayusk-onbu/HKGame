#include "StatusComponent.h"


float StatusComponent::GetCurrentSpeed() const {
    float multiplier = 1.0f;
    // activeEffects_ を走査して、Speed 系のバフ・デバフを合算
    for (auto& effect : activeEffects_) {
        if (effect.statusType == StatusType::Speed) {
            multiplier *= effect.multiplier;
        }
    }
    return baseSpeed_ * multiplier;
}