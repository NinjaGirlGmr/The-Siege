#pragma once

#include <SFML/Graphics.hpp>

#include <array>

class EnemyDeathEffect : public sf::Drawable
{
public:
    EnemyDeathEffect(sf::Vector2f centerPosition, sf::Vector2f effectSize, sf::Color effectColor);

    void update(float deltaTime);
    bool isFinished() const;

private:
    struct EffectParticle
    {
        sf::RectangleShape shape;
        sf::Vector2f velocity;
    };

    void draw(sf::RenderTarget &target, sf::RenderStates states) const override;

    static constexpr std::size_t ParticleCount = 6;

    std::array<EffectParticle, ParticleCount> particles_;
    sf::RectangleShape whiteFlashShape_;
    float elapsedSeconds_ = 0.0f;
};
