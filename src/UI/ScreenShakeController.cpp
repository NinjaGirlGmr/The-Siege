#include "ScreenShakeController.hpp"

#include <algorithm>
#include <cmath>

void ScreenShakeController::addShake(float durationSeconds, float strength)
{
    // Keep the strongest requested shake so overlapping impacts still feel like one clear response.
    remainingShakeSeconds_ = std::max(remainingShakeSeconds_, durationSeconds);
    shakeStrength_ = std::max(shakeStrength_, strength);
}

void ScreenShakeController::update(float deltaTime)
{
    // The controller owns the decay, so gameplay code only asks for shake bursts and does not
    // need to manually manage timers every frame.
    remainingShakeSeconds_ = std::max(0.0f, remainingShakeSeconds_ - deltaTime);
    if (remainingShakeSeconds_ <= 0.0f)
    {
        shakeStrength_ = 0.0f;
    }
}

sf::Vector2f ScreenShakeController::getCurrentOffset(float elapsedSceneTimeSeconds) const
{
    if (remainingShakeSeconds_ <= 0.0f || shakeStrength_ <= 0.0f)
    {
        return {};
    }

    // Use two different frequencies so the camera feels organic instead of vibrating in one axis.
    // The vertical shake is deliberately weaker so the screen does not feel too chaotic.
    return {
        std::sin(elapsedSceneTimeSeconds * 52.0f) * shakeStrength_,
        std::cos(elapsedSceneTimeSeconds * 68.0f) * shakeStrength_ * 0.35f,
    };
}

void ScreenShakeController::reset()
{
    // Reset between levels/retries so an old impact never leaks into a fresh attempt.
    remainingShakeSeconds_ = 0.0f;
    shakeStrength_ = 0.0f;
}
