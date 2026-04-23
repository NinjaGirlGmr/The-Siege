#pragma once

#include <SFML/Graphics.hpp>

class ScreenShakeController
{
public:
    void addShake(float durationSeconds, float strength);
    void update(float deltaTime);
    sf::Vector2f getCurrentOffset(float elapsedSceneTimeSeconds) const;
    void reset();

private:
    float remainingShakeSeconds_ = 0.0f;
    float shakeStrength_ = 0.0f;
};
