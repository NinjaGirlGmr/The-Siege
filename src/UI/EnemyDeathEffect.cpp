#include "EnemyDeathEffect.hpp"

#include <algorithm>
#include <array>

namespace
{
// These timings are intentionally short so kills feel punchy without blocking the action.
constexpr float EffectLifetimeSeconds = 0.32f;
constexpr float FlashLifetimeSeconds = 0.08f;
// The temporary particle directions create a quick outward burst. This can be replaced later
// with sprite animation or a more advanced particle system without changing gameplay code.
constexpr std::array<sf::Vector2f, 6> ParticleDirections = {{
    {-1.0f, -0.8f},
    {-0.45f, -1.0f},
    {0.45f, -1.0f},
    {1.0f, -0.8f},
    {-0.7f, 0.55f},
    {0.7f, 0.55f},
}};
}

EnemyDeathEffect::EnemyDeathEffect(
    sf::Vector2f centerPosition,
    sf::Vector2f effectSize,
    sf::Color effectColor)
{
    // These temporary rectangles are standing in for future sprite-based explosion art.
    // The white flash gives a clear "hit pop" before the colored pieces spread outward.
    whiteFlashShape_.setSize(effectSize * 1.2f);
    whiteFlashShape_.setOrigin(whiteFlashShape_.getSize() * 0.5f);
    whiteFlashShape_.setPosition(centerPosition);
    whiteFlashShape_.setFillColor(sf::Color::White);

    const sf::Vector2f particleSize = {
        std::max(6.0f, effectSize.x * 0.18f),
        std::max(6.0f, effectSize.y * 0.18f),
    };

    for (std::size_t particleIndex = 0; particleIndex < particles_.size(); ++particleIndex)
    {
        // Each particle starts centered on the enemy and then separates using its own direction vector.
        particles_[particleIndex].shape.setSize(particleSize);
        particles_[particleIndex].shape.setOrigin(particleSize * 0.5f);
        particles_[particleIndex].shape.setPosition(centerPosition);
        particles_[particleIndex].shape.setFillColor(effectColor);
        particles_[particleIndex].velocity = ParticleDirections[particleIndex] * 120.0f;
    }
}

void EnemyDeathEffect::update(float deltaTime)
{
    elapsedSeconds_ += deltaTime;

    // Fade and shrink the colored pieces over the full lifetime so the burst quickly reads and clears.
    const float lifetimeProgress = std::clamp(elapsedSeconds_ / EffectLifetimeSeconds, 0.0f, 1.0f);
    for (EffectParticle &particle : particles_)
    {
        particle.shape.move(particle.velocity * deltaTime);
        particle.shape.rotate(sf::degrees(220.0f * deltaTime));
        particle.shape.scale({1.0f - 0.7f * deltaTime, 1.0f - 0.7f * deltaTime});
        const sf::Color currentColor = particle.shape.getFillColor();
        particle.shape.setFillColor(sf::Color(
            currentColor.r,
            currentColor.g,
            currentColor.b,
            static_cast<std::uint8_t>(255.0f * (1.0f - lifetimeProgress))));
    }

    // The flash disappears faster than the particles so the eye catches a bright kill frame first.
    const float flashProgress = std::clamp(elapsedSeconds_ / FlashLifetimeSeconds, 0.0f, 1.0f);
    whiteFlashShape_.setFillColor(sf::Color(
        255,
        255,
        255,
        static_cast<std::uint8_t>(180.0f * (1.0f - flashProgress))));
}

bool EnemyDeathEffect::isFinished() const
{
    return elapsedSeconds_ >= EffectLifetimeSeconds;
}

void EnemyDeathEffect::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
    // Draw the flash first so the colored shards appear on top of it.
    target.draw(whiteFlashShape_, states);
    for (const EffectParticle &particle : particles_)
    {
        target.draw(particle.shape, states);
    }
}
