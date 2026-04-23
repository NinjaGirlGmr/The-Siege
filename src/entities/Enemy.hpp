#pragma once

#include "Entity.hpp"

#include <array>

class Enemy : public Entity
{
public:
    enum class EnemyType
    {
        BasicGrunt,
        MiniBoss,
        FinalBoss,
    };

    // EnemyBehaviorProfile describes how a non-boss enemy should feel in combat.
    // The main loop can then talk in terms of "shooter" or "shielded" instead of
    // duplicating special-case logic every time a projectile collides.
    enum class EnemyBehaviorProfile
    {
        Basic,
        Shielded,
        Shooter,
        Agile,
    };

    struct HitResult
    {
        bool attackWasBlocked = false;
        bool shieldWasBroken = false;
        bool enemyTookDamage = false;
        bool enemyWasDefeated = false;
        int damageApplied = 0;
    };

    Enemy(
        sf::Vector2f position,
        EnemyType enemyType,
        int startingHealth,
        EnemyBehaviorProfile behaviorProfile = EnemyBehaviorProfile::Basic);
    Enemy(const Enemy &other);
    Enemy &operator=(const Enemy &other);
    Enemy(Enemy &&other);
    Enemy &operator=(Enemy &&other);

    sf::FloatRect getBounds() const;
    EnemyType getEnemyType() const;
    EnemyBehaviorProfile getBehaviorProfile() const;
    bool canFireProjectiles() const;
    bool hasShield() const;
    void updateAttackCooldown(float deltaTime);
    void updateVisualAnimation(float deltaTime);
    void triggerDamageFlash();
    bool isReadyToFireProjectile() const;
    void resetAttackCooldown(float cooldownMultiplier = 1.0f);
    sf::Vector2f getProjectileSpawnPosition() const;
    float getFormationSpeedMultiplier() const;
    int consumeVolleyPatternIndex();
    HitResult applyPlayerProjectileHit(int damage);
    HitResult applyPlayerDashHit(int damage);

    static int getBaseHealthForEnemyType(EnemyType enemyType);
    static sf::Vector2f getSpriteSizeForEnemyType(EnemyType enemyType);
    static sf::Vector2f getSpriteSizeForConfiguration(
        EnemyType enemyType,
        EnemyBehaviorProfile behaviorProfile);

private:
    static constexpr int SpriteSheetCellSize = 64;
    static constexpr float AnimationFramesPerSecond = 8.0f;
    static constexpr std::size_t BasicAnimationFrameCount = 8;
    static constexpr std::size_t AgileAnimationFrameCount = 7;
    static constexpr std::size_t RangedAnimationFrameCount = 27;
    static constexpr std::size_t ShieldAnimationStateCount = 3;
    static constexpr std::size_t ShieldAnimationMaxFrameCount = 8;

    void applyTemporaryAppearance();
    bool loadBasicAnimationTexture();
    bool loadAgileAnimationTexture();
    bool loadRangedAnimationTexture();
    bool loadShieldAnimationTextures();
    void initializeBasicAnimationFrames();
    void initializeAgileAnimationFrames();
    void initializeRangedAnimationFrames();
    void initializeShieldAnimationFrames();
    bool isBasicAnimationConfiguration() const;
    bool isAgileAnimationConfiguration() const;
    bool isRangedAnimationConfiguration() const;
    bool isShieldAnimationConfiguration() const;
    bool usesBasicAnimationSprite() const;
    bool usesAgileAnimationSprite() const;
    bool usesRangedAnimationSprite() const;
    bool usesShieldAnimationSprite() const;
    void rebindAppearanceTexture();
    void advanceBasicAnimation(float deltaTime);
    void advanceAgileAnimation(float deltaTime);
    void advanceShieldAnimation(float deltaTime);
    void applyBasicAnimationFrame();
    void applyAgileAnimationFrame();
    void applyRangedAttackAnimationFrame();
    void applyShieldAnimationFrame();
    std::size_t getShieldAnimationStateIndex() const;
    float getAttackCooldownDuration() const;
    float getAttackWindupDuration() const;
    float getProjectileWarningProgress() const;
    float getHoverAmplitude() const;
    float getPulseStrength() const;
    sf::Color getBaseColor() const;
    sf::Color getShieldOverlayColor() const;

protected:
    void draw(sf::RenderTarget &target, sf::RenderStates states) const override;

    EnemyType enemyType_;
    EnemyBehaviorProfile behaviorProfile_ = EnemyBehaviorProfile::Basic;
    // Each enemy keeps its own temporary texture alive for the Entity sprite.
    sf::Texture temporaryTexture_;
    sf::Texture basicAnimationTexture_;
    sf::Texture agileAnimationTexture_;
    sf::Texture rangedAnimationTexture_;
    std::array<sf::Texture, ShieldAnimationStateCount> shieldAnimationTextures_;
    std::array<sf::IntRect, BasicAnimationFrameCount> basicAnimationFrames_;
    std::array<sf::IntRect, AgileAnimationFrameCount> agileAnimationFrames_;
    std::array<sf::IntRect, RangedAnimationFrameCount> rangedAnimationFrames_;
    std::array<
        std::array<sf::IntRect, ShieldAnimationMaxFrameCount>,
        ShieldAnimationStateCount>
        shieldAnimationFrames_;
    std::array<std::size_t, ShieldAnimationStateCount> shieldAnimationFrameCounts_;
    std::size_t currentAnimationFrameIndex_ = 0;
    std::size_t rangedAnimationFrameIndex_ = 0;
    std::size_t currentShieldAnimationStateIndex_ = 0;
    float animationFrameSecondsAccumulator_ = 0.0f;
    bool basicAnimationLoaded_ = false;
    bool agileAnimationLoaded_ = false;
    bool rangedAnimationLoaded_ = false;
    bool shieldAnimationLoaded_ = false;
    float secondsUntilNextProjectile_ = 0.0f;
    float animationTimeSeconds_ = 0.0f;
    float damageFlashSecondsRemaining_ = 0.0f;
    float attackCooldownDurationSeconds_ = 0.0f;
    float shieldFlashSecondsRemaining_ = 0.0f;
    int shieldStrength_ = 0;
    int volleyPatternIndex_ = 0;
};
