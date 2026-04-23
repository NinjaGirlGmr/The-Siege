#pragma once

#include <SFML/Graphics.hpp>

#include <optional>

class Projectile : public sf::Drawable
{
public:
    enum class VisualVariant
    {
        Player,
        EnemyShooter,
        MiniBoss,
        FinalBoss,
    };

    static constexpr int DefaultPlayerProjectileDamage = 10;
    static constexpr int DefaultEnemyProjectileDamage = 8;

    Projectile(
        sf::Vector2f position,
        sf::Vector2f velocity,
        int damage,
        float radius,
        sf::Color startColor,
        float launchDelaySeconds = 0.0f,
        int remainingPierceCount = 0,
        VisualVariant visualVariant = VisualVariant::Player);

    static Projectile createPlayerProjectile(
        sf::Vector2f position,
        float launchDelaySeconds = 0.0f);
    static Projectile createEnemyProjectile(sf::Vector2f position);
    static Projectile createEnemyProjectileTowardTarget(
        sf::Vector2f position,
        sf::Vector2f targetPosition,
        float speedMultiplier = 1.0f,
        float angleOffsetDegrees = 0.0f,
        VisualVariant visualVariant = VisualVariant::EnemyShooter);

    void update(float deltaTime);
    void destroy();
    bool isActive() const;
    bool isOffscreen() const;
    int getDamage() const;
    sf::FloatRect getBounds() const;
    bool consumePierceCharge();
    void nudgeForwardAfterPierce(float distance);

private:
    void draw(sf::RenderTarget &target, sf::RenderStates states) const override;
    void initializeSprite(sf::Color fallbackColor, VisualVariant visualVariant);
    void updateSpriteFrame();
    void syncSpriteTransform();

    sf::CircleShape body_;
    std::optional<sf::Sprite> sprite_;
    sf::Vector2f velocity_;
    int damage_ = DefaultPlayerProjectileDamage;
    int remainingPierceCount_ = 0;
    float launchDelaySecondsRemaining_ = 0.0f;
    float animationTimer_ = 0.0f;
    bool active_ = true;
};
