#include "Enemy.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <utility>

namespace
{
constexpr float DamageFlashDurationSeconds = 0.1f;
constexpr float ShieldFlashDurationSeconds = 0.12f;
constexpr float BasicEnemyVisualScaleMultiplier = 2.0f;
constexpr float AgileEnemyVisualScaleMultiplier = 2.0f;
constexpr float RangedEnemyVisualScaleMultiplier = 2.0f;
constexpr float ShieldEnemyVisualScaleMultiplier = 2.0f;
constexpr int RangedEnemySpriteSheetColumns = 5;
constexpr int RangedEnemySpriteSheetRows = 6;
constexpr int ShieldEnemySpriteSheetColumns = 3;
constexpr int ShieldEnemySpriteSheetRows = 3;
constexpr std::size_t ShieldEnemyAnimationStateCount = 3;
constexpr std::array<const char *, 2> BasicEnemySpriteSheetPaths = {
    "resources/sprites/Enemy1.png",
    "../resources/sprites/Enemy1.png",
};
constexpr std::array<const char *, 2> AgileEnemySpriteSheetPaths = {
    "resources/sprites/AgileEnemy.png",
    "../resources/sprites/AgileEnemy.png",
};
constexpr std::array<const char *, 2> RangedEnemySpriteSheetPaths = {
    "resources/sprites/EnemyRange.png",
    "../resources/sprites/EnemyRange.png",
};
constexpr std::array<std::array<const char *, 2>, ShieldEnemyAnimationStateCount> ShieldEnemySpriteSheetPaths = {{
    {"resources/sprites/Sheild1.png", "../resources/sprites/Sheild1.png"},
    {"resources/sprites/Shield2.png", "../resources/sprites/Shield2.png"},
    {"resources/sprites/Shield3.png", "../resources/sprites/Shield3.png"},
}};
constexpr std::array<std::size_t, ShieldEnemyAnimationStateCount> ShieldEnemyAnimationFrameCounts = {
    8,
    7,
    7,
};

sf::Vector2f getEnemySpriteSizeForType(
    Enemy::EnemyType enemyType,
    Enemy::EnemyBehaviorProfile behaviorProfile)
{
    switch (enemyType)
    {
        case Enemy::EnemyType::BasicGrunt:
            if (behaviorProfile == Enemy::EnemyBehaviorProfile::Basic)
            {
                return {64.0f, 64.0f};
            }
            if (behaviorProfile == Enemy::EnemyBehaviorProfile::Shooter)
            {
                return {64.0f, 64.0f};
            }
            if (behaviorProfile == Enemy::EnemyBehaviorProfile::Shielded ||
                behaviorProfile == Enemy::EnemyBehaviorProfile::Agile)
            {
                return {64.0f, 64.0f};
            }
            return {48.0f, 32.0f};
        case Enemy::EnemyType::MiniBoss:
            return {160.0f, 88.0f};
        case Enemy::EnemyType::FinalBoss:
            return {240.0f, 120.0f};
    }

    return {48.0f, 32.0f};
}
}

Enemy::Enemy(
    sf::Vector2f position,
    EnemyType enemyType,
    int startingHealth,
    EnemyBehaviorProfile behaviorProfile)
    : Entity(
          startingHealth,
          0,
          0.0f,
          getEnemySpriteSizeForType(enemyType, behaviorProfile)),
      enemyType_(enemyType),
      behaviorProfile_(behaviorProfile)
{
    // Bosses keep their inherited behavior even if the caller forgets to pass one.
    if (enemyType_ == EnemyType::MiniBoss || enemyType_ == EnemyType::FinalBoss)
    {
        behaviorProfile_ = EnemyBehaviorProfile::Shooter;
    }

    // Shielded enemies are still ordinary grunts, but they need an extra hit-state
    // so the player can learn "shots bounce, dash breaks through" on sight.
    if (behaviorProfile_ == EnemyBehaviorProfile::Shielded)
    {
        shieldStrength_ = 2;
    }

    attackCooldownDurationSeconds_ = getAttackCooldownDuration();
    if (isBasicAnimationConfiguration() && loadBasicAnimationTexture())
    {
        basicAnimationLoaded_ = true;
        initializeBasicAnimationFrames();
        setTexture(basicAnimationTexture_);
        applyBasicAnimationFrame();
    }
    else if (isAgileAnimationConfiguration() && loadAgileAnimationTexture())
    {
        agileAnimationLoaded_ = true;
        initializeAgileAnimationFrames();
        setTexture(agileAnimationTexture_);
        applyAgileAnimationFrame();
    }
    else if (isShieldAnimationConfiguration() && loadShieldAnimationTextures())
    {
        shieldAnimationLoaded_ = true;
        initializeShieldAnimationFrames();
        currentShieldAnimationStateIndex_ = getShieldAnimationStateIndex();
        setTexture(shieldAnimationTextures_[currentShieldAnimationStateIndex_]);
        applyShieldAnimationFrame();
    }
    else if (isRangedAnimationConfiguration() && loadRangedAnimationTexture())
    {
        rangedAnimationLoaded_ = true;
        initializeRangedAnimationFrames();
        setTexture(rangedAnimationTexture_);
        applyRangedAttackAnimationFrame();
    }
    else
    {
        applyTemporaryAppearance();
    }
    setPosition(position);
    resetAttackCooldown();
    applyRangedAttackAnimationFrame();
}

Enemy::Enemy(const Enemy &other)
    : Entity(other),
      enemyType_(other.enemyType_),
      behaviorProfile_(other.behaviorProfile_),
      temporaryTexture_(other.temporaryTexture_),
      basicAnimationTexture_(other.basicAnimationTexture_),
      agileAnimationTexture_(other.agileAnimationTexture_),
      rangedAnimationTexture_(other.rangedAnimationTexture_),
      shieldAnimationTextures_(other.shieldAnimationTextures_),
      basicAnimationFrames_(other.basicAnimationFrames_),
      agileAnimationFrames_(other.agileAnimationFrames_),
      rangedAnimationFrames_(other.rangedAnimationFrames_),
      shieldAnimationFrames_(other.shieldAnimationFrames_),
      shieldAnimationFrameCounts_(other.shieldAnimationFrameCounts_),
      currentAnimationFrameIndex_(other.currentAnimationFrameIndex_),
      rangedAnimationFrameIndex_(other.rangedAnimationFrameIndex_),
      currentShieldAnimationStateIndex_(other.currentShieldAnimationStateIndex_),
      animationFrameSecondsAccumulator_(other.animationFrameSecondsAccumulator_),
      basicAnimationLoaded_(other.basicAnimationLoaded_),
      agileAnimationLoaded_(other.agileAnimationLoaded_),
      rangedAnimationLoaded_(other.rangedAnimationLoaded_),
      shieldAnimationLoaded_(other.shieldAnimationLoaded_),
      secondsUntilNextProjectile_(other.secondsUntilNextProjectile_),
      animationTimeSeconds_(other.animationTimeSeconds_),
      damageFlashSecondsRemaining_(other.damageFlashSecondsRemaining_),
      attackCooldownDurationSeconds_(other.attackCooldownDurationSeconds_),
      shieldFlashSecondsRemaining_(other.shieldFlashSecondsRemaining_),
      shieldStrength_(other.shieldStrength_),
      volleyPatternIndex_(other.volleyPatternIndex_)
{
    rebindAppearanceTexture();
}

Enemy &Enemy::operator=(const Enemy &other)
{
    if (this == &other)
    {
        return *this;
    }

    Entity::operator=(other);
    enemyType_ = other.enemyType_;
    behaviorProfile_ = other.behaviorProfile_;
    temporaryTexture_ = other.temporaryTexture_;
    basicAnimationTexture_ = other.basicAnimationTexture_;
    agileAnimationTexture_ = other.agileAnimationTexture_;
    rangedAnimationTexture_ = other.rangedAnimationTexture_;
    shieldAnimationTextures_ = other.shieldAnimationTextures_;
    basicAnimationFrames_ = other.basicAnimationFrames_;
    agileAnimationFrames_ = other.agileAnimationFrames_;
    rangedAnimationFrames_ = other.rangedAnimationFrames_;
    shieldAnimationFrames_ = other.shieldAnimationFrames_;
    shieldAnimationFrameCounts_ = other.shieldAnimationFrameCounts_;
    currentAnimationFrameIndex_ = other.currentAnimationFrameIndex_;
    rangedAnimationFrameIndex_ = other.rangedAnimationFrameIndex_;
    currentShieldAnimationStateIndex_ = other.currentShieldAnimationStateIndex_;
    animationFrameSecondsAccumulator_ = other.animationFrameSecondsAccumulator_;
    basicAnimationLoaded_ = other.basicAnimationLoaded_;
    agileAnimationLoaded_ = other.agileAnimationLoaded_;
    rangedAnimationLoaded_ = other.rangedAnimationLoaded_;
    shieldAnimationLoaded_ = other.shieldAnimationLoaded_;
    secondsUntilNextProjectile_ = other.secondsUntilNextProjectile_;
    animationTimeSeconds_ = other.animationTimeSeconds_;
    damageFlashSecondsRemaining_ = other.damageFlashSecondsRemaining_;
    attackCooldownDurationSeconds_ = other.attackCooldownDurationSeconds_;
    shieldFlashSecondsRemaining_ = other.shieldFlashSecondsRemaining_;
    shieldStrength_ = other.shieldStrength_;
    volleyPatternIndex_ = other.volleyPatternIndex_;
    rebindAppearanceTexture();
    return *this;
}

Enemy::Enemy(Enemy &&other)
    : Entity(std::move(other)),
      enemyType_(other.enemyType_),
      behaviorProfile_(other.behaviorProfile_),
      temporaryTexture_(std::move(other.temporaryTexture_)),
      basicAnimationTexture_(std::move(other.basicAnimationTexture_)),
      agileAnimationTexture_(std::move(other.agileAnimationTexture_)),
      rangedAnimationTexture_(std::move(other.rangedAnimationTexture_)),
      shieldAnimationTextures_(std::move(other.shieldAnimationTextures_)),
      basicAnimationFrames_(std::move(other.basicAnimationFrames_)),
      agileAnimationFrames_(std::move(other.agileAnimationFrames_)),
      rangedAnimationFrames_(std::move(other.rangedAnimationFrames_)),
      shieldAnimationFrames_(std::move(other.shieldAnimationFrames_)),
      shieldAnimationFrameCounts_(std::move(other.shieldAnimationFrameCounts_)),
      currentAnimationFrameIndex_(other.currentAnimationFrameIndex_),
      rangedAnimationFrameIndex_(other.rangedAnimationFrameIndex_),
      currentShieldAnimationStateIndex_(other.currentShieldAnimationStateIndex_),
      animationFrameSecondsAccumulator_(other.animationFrameSecondsAccumulator_),
      basicAnimationLoaded_(other.basicAnimationLoaded_),
      agileAnimationLoaded_(other.agileAnimationLoaded_),
      rangedAnimationLoaded_(other.rangedAnimationLoaded_),
      shieldAnimationLoaded_(other.shieldAnimationLoaded_),
      secondsUntilNextProjectile_(other.secondsUntilNextProjectile_),
      animationTimeSeconds_(other.animationTimeSeconds_),
      damageFlashSecondsRemaining_(other.damageFlashSecondsRemaining_),
      attackCooldownDurationSeconds_(other.attackCooldownDurationSeconds_),
      shieldFlashSecondsRemaining_(other.shieldFlashSecondsRemaining_),
      shieldStrength_(other.shieldStrength_),
      volleyPatternIndex_(other.volleyPatternIndex_)
{
    rebindAppearanceTexture();
}

Enemy &Enemy::operator=(Enemy &&other)
{
    if (this == &other)
    {
        return *this;
    }

    Entity::operator=(std::move(other));
    enemyType_ = other.enemyType_;
    behaviorProfile_ = other.behaviorProfile_;
    temporaryTexture_ = std::move(other.temporaryTexture_);
    basicAnimationTexture_ = std::move(other.basicAnimationTexture_);
    agileAnimationTexture_ = std::move(other.agileAnimationTexture_);
    rangedAnimationTexture_ = std::move(other.rangedAnimationTexture_);
    shieldAnimationTextures_ = std::move(other.shieldAnimationTextures_);
    basicAnimationFrames_ = std::move(other.basicAnimationFrames_);
    agileAnimationFrames_ = std::move(other.agileAnimationFrames_);
    rangedAnimationFrames_ = std::move(other.rangedAnimationFrames_);
    shieldAnimationFrames_ = std::move(other.shieldAnimationFrames_);
    shieldAnimationFrameCounts_ = std::move(other.shieldAnimationFrameCounts_);
    currentAnimationFrameIndex_ = other.currentAnimationFrameIndex_;
    rangedAnimationFrameIndex_ = other.rangedAnimationFrameIndex_;
    currentShieldAnimationStateIndex_ = other.currentShieldAnimationStateIndex_;
    animationFrameSecondsAccumulator_ = other.animationFrameSecondsAccumulator_;
    basicAnimationLoaded_ = other.basicAnimationLoaded_;
    agileAnimationLoaded_ = other.agileAnimationLoaded_;
    rangedAnimationLoaded_ = other.rangedAnimationLoaded_;
    shieldAnimationLoaded_ = other.shieldAnimationLoaded_;
    secondsUntilNextProjectile_ = other.secondsUntilNextProjectile_;
    animationTimeSeconds_ = other.animationTimeSeconds_;
    damageFlashSecondsRemaining_ = other.damageFlashSecondsRemaining_;
    attackCooldownDurationSeconds_ = other.attackCooldownDurationSeconds_;
    shieldFlashSecondsRemaining_ = other.shieldFlashSecondsRemaining_;
    shieldStrength_ = other.shieldStrength_;
    volleyPatternIndex_ = other.volleyPatternIndex_;
    rebindAppearanceTexture();
    return *this;
}

sf::FloatRect Enemy::getBounds() const
{
    const sf::Sprite *sprite = getSprite();
    if (!sprite)
    {
        return {};
    }

    return sprite->getGlobalBounds();
}

Enemy::EnemyType Enemy::getEnemyType() const
{
    return enemyType_;
}

Enemy::EnemyBehaviorProfile Enemy::getBehaviorProfile() const
{
    return behaviorProfile_;
}

bool Enemy::canFireProjectiles() const
{
    return behaviorProfile_ == EnemyBehaviorProfile::Shooter ||
           enemyType_ == EnemyType::MiniBoss ||
           enemyType_ == EnemyType::FinalBoss;
}

bool Enemy::hasShield() const
{
    return shieldStrength_ > 0;
}

void Enemy::updateAttackCooldown(float deltaTime)
{
    if (!canFireProjectiles())
    {
        return;
    }

    secondsUntilNextProjectile_ = std::max(0.0f, secondsUntilNextProjectile_ - deltaTime);
    applyRangedAttackAnimationFrame();
}

void Enemy::updateVisualAnimation(float deltaTime)
{
    animationTimeSeconds_ += deltaTime;
    advanceBasicAnimation(deltaTime);
    advanceAgileAnimation(deltaTime);
    advanceShieldAnimation(deltaTime);
    damageFlashSecondsRemaining_ = std::max(0.0f, damageFlashSecondsRemaining_ - deltaTime);
    shieldFlashSecondsRemaining_ = std::max(0.0f, shieldFlashSecondsRemaining_ - deltaTime);
}

void Enemy::triggerDamageFlash()
{
    damageFlashSecondsRemaining_ = DamageFlashDurationSeconds;
}

bool Enemy::isReadyToFireProjectile() const
{
    return canFireProjectiles() && secondsUntilNextProjectile_ <= 0.0f;
}

void Enemy::resetAttackCooldown(float cooldownMultiplier)
{
    secondsUntilNextProjectile_ =
        attackCooldownDurationSeconds_ * std::max(0.1f, cooldownMultiplier);
}

sf::Vector2f Enemy::getProjectileSpawnPosition() const
{
    const sf::Sprite *sprite = getSprite();
    if (!sprite)
    {
        return {};
    }

    const sf::FloatRect bounds = sprite->getGlobalBounds();
    return {
        bounds.position.x + bounds.size.x * 0.5f,
        bounds.position.y + bounds.size.y,
    };
}

float Enemy::getFormationSpeedMultiplier() const
{
    if (behaviorProfile_ == EnemyBehaviorProfile::Agile)
    {
        return 1.35f;
    }

    return 1.0f;
}

int Enemy::consumeVolleyPatternIndex()
{
    const int volleyPatternIndex = volleyPatternIndex_;
    ++volleyPatternIndex_;
    return volleyPatternIndex;
}

Enemy::HitResult Enemy::applyPlayerProjectileHit(int damage)
{
    HitResult hitResult;
    if (!isAlive())
    {
        return hitResult;
    }

    if (hasShield())
    {
        shieldStrength_ = std::max(0, shieldStrength_ - 1);
        shieldFlashSecondsRemaining_ = ShieldFlashDurationSeconds;
        applyShieldAnimationFrame();
        hitResult.attackWasBlocked = true;
        hitResult.shieldWasBroken = shieldStrength_ == 0;
        return hitResult;
    }

    const int previousHealth = getHealth();
    takeDamage(damage);
    hitResult.enemyTookDamage = getHealth() < previousHealth;
    hitResult.damageApplied = previousHealth - getHealth();
    hitResult.enemyWasDefeated = !isAlive();
    return hitResult;
}

Enemy::HitResult Enemy::applyPlayerDashHit(int damage)
{
    HitResult hitResult;
    if (!isAlive())
    {
        return hitResult;
    }

    if (hasShield())
    {
        // Dash is the hard counter for early shield enemies. Breaking the guard first
        // teaches the player that mobility is part of offense, not only defense.
        shieldStrength_ = 0;
        shieldFlashSecondsRemaining_ = ShieldFlashDurationSeconds;
        applyShieldAnimationFrame();
        hitResult.shieldWasBroken = true;
    }

    const int previousHealth = getHealth();
    takeDamage(damage);
    hitResult.enemyTookDamage = getHealth() < previousHealth;
    hitResult.damageApplied = previousHealth - getHealth();
    hitResult.enemyWasDefeated = !isAlive();
    return hitResult;
}

int Enemy::getBaseHealthForEnemyType(EnemyType enemyType)
{
    switch (enemyType)
    {
        case EnemyType::BasicGrunt:
            return 10;
        case EnemyType::MiniBoss:
            return 80;
        case EnemyType::FinalBoss:
            return 160;
    }

    return 10;
}

sf::Vector2f Enemy::getSpriteSizeForEnemyType(EnemyType enemyType)
{
    return getEnemySpriteSizeForType(enemyType, EnemyBehaviorProfile::Basic);
}

sf::Vector2f Enemy::getSpriteSizeForConfiguration(
    EnemyType enemyType,
    EnemyBehaviorProfile behaviorProfile)
{
    return getEnemySpriteSizeForType(enemyType, behaviorProfile);
}

void Enemy::applyTemporaryAppearance()
{
    const sf::Vector2f spriteSize = getSpriteSize();
    const sf::Image temporaryImage(
        {static_cast<unsigned int>(spriteSize.x), static_cast<unsigned int>(spriteSize.y)},
        getBaseColor());

    if (temporaryTexture_.loadFromImage(temporaryImage))
    {
        setTexture(temporaryTexture_);
    }
}

bool Enemy::loadBasicAnimationTexture()
{
    for (const char *spriteSheetPath : BasicEnemySpriteSheetPaths)
    {
        if (!std::filesystem::exists(spriteSheetPath))
        {
            continue;
        }

        if (!basicAnimationTexture_.loadFromFile(spriteSheetPath))
        {
            continue;
        }

        basicAnimationTexture_.setSmooth(false);
        return true;
    }

    return false;
}

bool Enemy::loadAgileAnimationTexture()
{
    for (const char *spriteSheetPath : AgileEnemySpriteSheetPaths)
    {
        if (!std::filesystem::exists(spriteSheetPath))
        {
            continue;
        }

        if (!agileAnimationTexture_.loadFromFile(spriteSheetPath))
        {
            continue;
        }

        agileAnimationTexture_.setSmooth(false);
        return true;
    }

    return false;
}

bool Enemy::loadRangedAnimationTexture()
{
    for (const char *spriteSheetPath : RangedEnemySpriteSheetPaths)
    {
        if (!std::filesystem::exists(spriteSheetPath))
        {
            continue;
        }

        if (!rangedAnimationTexture_.loadFromFile(spriteSheetPath))
        {
            continue;
        }

        rangedAnimationTexture_.setSmooth(false);
        return true;
    }

    return false;
}

bool Enemy::loadShieldAnimationTextures()
{
    for (std::size_t stateIndex = 0; stateIndex < ShieldAnimationStateCount; ++stateIndex)
    {
        bool loadedStateTexture = false;
        for (const char *spriteSheetPath : ShieldEnemySpriteSheetPaths[stateIndex])
        {
            if (!std::filesystem::exists(spriteSheetPath))
            {
                continue;
            }

            if (!shieldAnimationTextures_[stateIndex].loadFromFile(spriteSheetPath))
            {
                continue;
            }

            shieldAnimationTextures_[stateIndex].setSmooth(false);
            loadedStateTexture = true;
            break;
        }

        if (!loadedStateTexture)
        {
            return false;
        }
    }

    return true;
}

void Enemy::initializeBasicAnimationFrames()
{
    std::size_t registeredFrameCount = 0;
    for (int rowIndex = 0; rowIndex < 3 && registeredFrameCount < BasicAnimationFrameCount; ++rowIndex)
    {
        for (int columnIndex = 0; columnIndex < 3 && registeredFrameCount < BasicAnimationFrameCount; ++columnIndex)
        {
            basicAnimationFrames_[registeredFrameCount] = sf::IntRect(
                sf::Vector2i{columnIndex * SpriteSheetCellSize, rowIndex * SpriteSheetCellSize},
                sf::Vector2i{SpriteSheetCellSize, SpriteSheetCellSize});
            ++registeredFrameCount;
        }
    }
}

void Enemy::initializeAgileAnimationFrames()
{
    std::size_t registeredFrameCount = 0;
    for (int rowIndex = 0; rowIndex < 3 && registeredFrameCount < AgileAnimationFrameCount; ++rowIndex)
    {
        for (int columnIndex = 0; columnIndex < 3 && registeredFrameCount < AgileAnimationFrameCount; ++columnIndex)
        {
            agileAnimationFrames_[registeredFrameCount] = sf::IntRect(
                sf::Vector2i{columnIndex * SpriteSheetCellSize, rowIndex * SpriteSheetCellSize},
                sf::Vector2i{SpriteSheetCellSize, SpriteSheetCellSize});
            ++registeredFrameCount;
        }
    }
}

void Enemy::initializeRangedAnimationFrames()
{
    std::size_t registeredFrameCount = 0;
    for (int rowIndex = RangedEnemySpriteSheetRows - 1;
         rowIndex >= 0 && registeredFrameCount < RangedAnimationFrameCount;
         --rowIndex)
    {
        const int lastColumnIndex =
            rowIndex == RangedEnemySpriteSheetRows - 1 ? 1 : RangedEnemySpriteSheetColumns - 1;
        for (int columnIndex = lastColumnIndex;
             columnIndex >= 0 && registeredFrameCount < RangedAnimationFrameCount;
             --columnIndex)
        {
            rangedAnimationFrames_[registeredFrameCount] = sf::IntRect(
                sf::Vector2i{columnIndex * SpriteSheetCellSize, rowIndex * SpriteSheetCellSize},
                sf::Vector2i{SpriteSheetCellSize, SpriteSheetCellSize});
            ++registeredFrameCount;
        }
    }
}

void Enemy::initializeShieldAnimationFrames()
{
    shieldAnimationFrameCounts_ = ShieldEnemyAnimationFrameCounts;

    for (std::size_t stateIndex = 0; stateIndex < ShieldAnimationStateCount; ++stateIndex)
    {
        std::size_t registeredFrameCount = 0;
        for (int rowIndex = 0;
             rowIndex < ShieldEnemySpriteSheetRows &&
             registeredFrameCount < shieldAnimationFrameCounts_[stateIndex];
             ++rowIndex)
        {
            for (int columnIndex = 0;
                 columnIndex < ShieldEnemySpriteSheetColumns &&
                 registeredFrameCount < shieldAnimationFrameCounts_[stateIndex];
                 ++columnIndex)
            {
                shieldAnimationFrames_[stateIndex][registeredFrameCount] = sf::IntRect(
                    sf::Vector2i{columnIndex * SpriteSheetCellSize, rowIndex * SpriteSheetCellSize},
                    sf::Vector2i{SpriteSheetCellSize, SpriteSheetCellSize});
                ++registeredFrameCount;
            }
        }
    }
}

bool Enemy::isBasicAnimationConfiguration() const
{
    return enemyType_ == EnemyType::BasicGrunt &&
           behaviorProfile_ == EnemyBehaviorProfile::Basic;
}

bool Enemy::isAgileAnimationConfiguration() const
{
    return enemyType_ == EnemyType::BasicGrunt &&
           behaviorProfile_ == EnemyBehaviorProfile::Agile;
}

bool Enemy::isRangedAnimationConfiguration() const
{
    return enemyType_ == EnemyType::BasicGrunt &&
           behaviorProfile_ == EnemyBehaviorProfile::Shooter;
}

bool Enemy::isShieldAnimationConfiguration() const
{
    return enemyType_ == EnemyType::BasicGrunt &&
           behaviorProfile_ == EnemyBehaviorProfile::Shielded;
}

bool Enemy::usesBasicAnimationSprite() const
{
    return basicAnimationLoaded_ && isBasicAnimationConfiguration();
}

bool Enemy::usesAgileAnimationSprite() const
{
    return agileAnimationLoaded_ && isAgileAnimationConfiguration();
}

bool Enemy::usesRangedAnimationSprite() const
{
    return rangedAnimationLoaded_ && isRangedAnimationConfiguration();
}

bool Enemy::usesShieldAnimationSprite() const
{
    return shieldAnimationLoaded_ && isShieldAnimationConfiguration();
}

void Enemy::rebindAppearanceTexture()
{
    if (usesBasicAnimationSprite())
    {
        setTexture(basicAnimationTexture_, false);
        applyBasicAnimationFrame();
        return;
    }

    if (usesAgileAnimationSprite())
    {
        setTexture(agileAnimationTexture_, false);
        applyAgileAnimationFrame();
        return;
    }

    if (usesRangedAnimationSprite())
    {
        setTexture(rangedAnimationTexture_, false);
        applyRangedAttackAnimationFrame();
        return;
    }

    if (usesShieldAnimationSprite())
    {
        currentShieldAnimationStateIndex_ = getShieldAnimationStateIndex();
        setTexture(shieldAnimationTextures_[currentShieldAnimationStateIndex_], false);
        applyShieldAnimationFrame();
        return;
    }

    setTexture(temporaryTexture_, false);
}

void Enemy::advanceBasicAnimation(float deltaTime)
{
    if (!usesBasicAnimationSprite())
    {
        return;
    }

    const float secondsPerFrame = 1.0f / AnimationFramesPerSecond;
    animationFrameSecondsAccumulator_ += deltaTime;

    while (animationFrameSecondsAccumulator_ >= secondsPerFrame)
    {
        animationFrameSecondsAccumulator_ -= secondsPerFrame;
        currentAnimationFrameIndex_ =
            (currentAnimationFrameIndex_ + 1) % BasicAnimationFrameCount;
    }

    applyBasicAnimationFrame();
}

void Enemy::advanceAgileAnimation(float deltaTime)
{
    if (!usesAgileAnimationSprite())
    {
        return;
    }

    const float secondsPerFrame = 1.0f / AnimationFramesPerSecond;
    animationFrameSecondsAccumulator_ += deltaTime;

    while (animationFrameSecondsAccumulator_ >= secondsPerFrame)
    {
        animationFrameSecondsAccumulator_ -= secondsPerFrame;
        currentAnimationFrameIndex_ =
            (currentAnimationFrameIndex_ + 1) % AgileAnimationFrameCount;
    }

    applyAgileAnimationFrame();
}

void Enemy::advanceShieldAnimation(float deltaTime)
{
    if (!usesShieldAnimationSprite())
    {
        return;
    }

    const std::size_t shieldAnimationStateIndex = getShieldAnimationStateIndex();
    if (shieldAnimationStateIndex != currentShieldAnimationStateIndex_)
    {
        currentShieldAnimationStateIndex_ = shieldAnimationStateIndex;
        currentAnimationFrameIndex_ = 0;
        animationFrameSecondsAccumulator_ = 0.0f;
        setTexture(shieldAnimationTextures_[currentShieldAnimationStateIndex_], true);
        applyShieldAnimationFrame();
        return;
    }

    const float secondsPerFrame = 1.0f / AnimationFramesPerSecond;
    animationFrameSecondsAccumulator_ += deltaTime;

    while (animationFrameSecondsAccumulator_ >= secondsPerFrame)
    {
        animationFrameSecondsAccumulator_ -= secondsPerFrame;
        currentAnimationFrameIndex_ =
            (currentAnimationFrameIndex_ + 1) %
            shieldAnimationFrameCounts_[currentShieldAnimationStateIndex_];
    }

    applyShieldAnimationFrame();
}

void Enemy::applyBasicAnimationFrame()
{
    if (!usesBasicAnimationSprite())
    {
        return;
    }

    setTextureRect(basicAnimationFrames_[currentAnimationFrameIndex_]);
}

void Enemy::applyAgileAnimationFrame()
{
    if (!usesAgileAnimationSprite())
    {
        return;
    }

    setTextureRect(agileAnimationFrames_[currentAnimationFrameIndex_]);
}

void Enemy::applyRangedAttackAnimationFrame()
{
    if (!usesRangedAnimationSprite())
    {
        return;
    }

    const float cooldownDuration = std::max(0.001f, attackCooldownDurationSeconds_);
    const float animationProgress =
        1.0f - std::clamp(secondsUntilNextProjectile_ / cooldownDuration, 0.0f, 1.0f);
    rangedAnimationFrameIndex_ = std::min(
        RangedAnimationFrameCount - 1,
        static_cast<std::size_t>(
            animationProgress * static_cast<float>(RangedAnimationFrameCount)));
    setTextureRect(rangedAnimationFrames_[rangedAnimationFrameIndex_]);
}

void Enemy::applyShieldAnimationFrame()
{
    if (!usesShieldAnimationSprite())
    {
        return;
    }

    const std::size_t shieldAnimationStateIndex = getShieldAnimationStateIndex();
    if (shieldAnimationStateIndex != currentShieldAnimationStateIndex_)
    {
        currentShieldAnimationStateIndex_ = shieldAnimationStateIndex;
        currentAnimationFrameIndex_ = 0;
        animationFrameSecondsAccumulator_ = 0.0f;
        setTexture(shieldAnimationTextures_[currentShieldAnimationStateIndex_], true);
    }

    const std::size_t frameCount = shieldAnimationFrameCounts_[currentShieldAnimationStateIndex_];
    currentAnimationFrameIndex_ %= frameCount;
    setTextureRect(shieldAnimationFrames_[currentShieldAnimationStateIndex_][currentAnimationFrameIndex_]);
}

std::size_t Enemy::getShieldAnimationStateIndex() const
{
    if (shieldStrength_ >= 2)
    {
        return 0;
    }

    if (shieldStrength_ == 1)
    {
        return 1;
    }

    return 2;
}

float Enemy::getAttackCooldownDuration() const
{
    if (enemyType_ == EnemyType::MiniBoss)
    {
        return 1.35f;
    }

    if (enemyType_ == EnemyType::FinalBoss)
    {
        return 0.9f;
    }

    switch (behaviorProfile_)
    {
        case EnemyBehaviorProfile::Basic:
            return 9999.0f;
        case EnemyBehaviorProfile::Shielded:
            return 9999.0f;
        case EnemyBehaviorProfile::Shooter:
            return 2.4f;
        case EnemyBehaviorProfile::Agile:
            return 9999.0f;
    }

    return 9999.0f;
}

float Enemy::getAttackWindupDuration() const
{
    if (enemyType_ == EnemyType::FinalBoss)
    {
        return 0.28f;
    }

    if (enemyType_ == EnemyType::MiniBoss)
    {
        return 0.36f;
    }

    if (behaviorProfile_ == EnemyBehaviorProfile::Shooter)
    {
        return 0.45f;
    }

    return 0.0f;
}

float Enemy::getProjectileWarningProgress() const
{
    if (!canFireProjectiles() || attackCooldownDurationSeconds_ >= 9999.0f)
    {
        return 0.0f;
    }

    const float windupDuration = getAttackWindupDuration();
    if (windupDuration <= 0.0f || secondsUntilNextProjectile_ > windupDuration)
    {
        return 0.0f;
    }

    return 1.0f - secondsUntilNextProjectile_ / windupDuration;
}

float Enemy::getHoverAmplitude() const
{
    if (usesBasicAnimationSprite())
    {
        return 0.75f;
    }

    if (usesAgileAnimationSprite())
    {
        return 0.75f;
    }

    if (usesRangedAnimationSprite())
    {
        return 0.75f;
    }

    if (usesShieldAnimationSprite())
    {
        return 0.75f;
    }

    switch (behaviorProfile_)
    {
        case EnemyBehaviorProfile::Basic:
            return 3.0f;
        case EnemyBehaviorProfile::Shielded:
            return 2.0f;
        case EnemyBehaviorProfile::Shooter:
            return 4.5f;
        case EnemyBehaviorProfile::Agile:
            return 6.0f;
    }

    return 3.0f;
}

float Enemy::getPulseStrength() const
{
    if (usesBasicAnimationSprite())
    {
        return 0.005f;
    }

    if (usesAgileAnimationSprite())
    {
        return 0.005f;
    }

    if (usesRangedAnimationSprite())
    {
        return 0.005f;
    }

    if (usesShieldAnimationSprite())
    {
        return 0.005f;
    }

    switch (behaviorProfile_)
    {
        case EnemyBehaviorProfile::Basic:
            return 0.04f;
        case EnemyBehaviorProfile::Shielded:
            return 0.02f;
        case EnemyBehaviorProfile::Shooter:
            return 0.05f;
        case EnemyBehaviorProfile::Agile:
            return 0.06f;
    }

    return 0.04f;
}

sf::Color Enemy::getBaseColor() const
{
    if (enemyType_ == EnemyType::MiniBoss)
    {
        return sf::Color(255, 170, 70);
    }

    if (enemyType_ == EnemyType::FinalBoss)
    {
        return sf::Color(220, 70, 90);
    }

    switch (behaviorProfile_)
    {
        case EnemyBehaviorProfile::Basic:
            return sf::Color(70, 210, 120);
        case EnemyBehaviorProfile::Shielded:
            return sf::Color(95, 145, 255);
        case EnemyBehaviorProfile::Shooter:
            return sf::Color(255, 120, 120);
        case EnemyBehaviorProfile::Agile:
            return sf::Color(255, 205, 90);
    }

    return sf::Color(70, 210, 120);
}

sf::Color Enemy::getShieldOverlayColor() const
{
    if (shieldFlashSecondsRemaining_ > 0.0f)
    {
        return sf::Color(255, 255, 255, 210);
    }

    return sf::Color(170, 220, 255, 185);
}

void Enemy::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
    const sf::Sprite *sprite = getSprite();
    if (!sprite)
    {
        return;
    }

    sf::Sprite posedSprite = *sprite;
    const sf::FloatRect localBounds = posedSprite.getLocalBounds();
    posedSprite.setOrigin({
        localBounds.position.x + localBounds.size.x * 0.5f,
        localBounds.position.y + localBounds.size.y * 0.5f,
    });

    const sf::Vector2f currentPosition = sprite->getPosition();
    const sf::FloatRect currentBounds = sprite->getGlobalBounds();
    const float bobOffset = std::sin(animationTimeSeconds_ * 2.6f) * getHoverAmplitude();
    const float pulseAmount =
        getPulseStrength() * (0.5f + 0.5f * std::sin(animationTimeSeconds_ * 4.0f));
    posedSprite.setPosition({
        currentPosition.x + currentBounds.size.x * 0.5f,
        currentPosition.y + currentBounds.size.y * 0.5f + bobOffset,
    });
    posedSprite.setScale({
        sprite->getScale().x * (1.0f + pulseAmount),
        sprite->getScale().y * (1.0f - pulseAmount * 0.7f),
    });
    if (usesBasicAnimationSprite())
    {
        posedSprite.setScale(posedSprite.getScale() * BasicEnemyVisualScaleMultiplier);
    }
    else if (usesAgileAnimationSprite())
    {
        posedSprite.setScale(posedSprite.getScale() * AgileEnemyVisualScaleMultiplier);
    }
    else if (usesRangedAnimationSprite())
    {
        posedSprite.setScale(posedSprite.getScale() * RangedEnemyVisualScaleMultiplier);
    }
    else if (usesShieldAnimationSprite())
    {
        posedSprite.setScale(posedSprite.getScale() * ShieldEnemyVisualScaleMultiplier);
    }

    if (damageFlashSecondsRemaining_ > 0.0f)
    {
        posedSprite.setColor(sf::Color::White);
    }
    else if (usesBasicAnimationSprite() || usesAgileAnimationSprite() ||
             usesRangedAnimationSprite() || usesShieldAnimationSprite())
    {
        posedSprite.setColor(sf::Color::White);
    }
    else
    {
        posedSprite.setColor(getBaseColor());
    }

    target.draw(posedSprite, states);

    // Shield overlay is drawn as an outline ring so the player can instantly spot
    // which enemies require a dash or extra planning before shooting.
    if (hasShield() && !usesShieldAnimationSprite())
    {
        sf::RectangleShape shieldOutline(currentBounds.size + sf::Vector2f(8.0f, 8.0f));
        shieldOutline.setOrigin(shieldOutline.getSize() * 0.5f);
        shieldOutline.setPosition(posedSprite.getPosition());
        shieldOutline.setFillColor(sf::Color::Transparent);
        shieldOutline.setOutlineThickness(3.0f);
        shieldOutline.setOutlineColor(getShieldOverlayColor());
        target.draw(shieldOutline, states);
    }

    // Shooters brighten during their windup so incoming danger is readable before
    // the projectile appears. That keeps the dodge-focused levels teachable.
    const float warningProgress = getProjectileWarningProgress();
    if (warningProgress > 0.0f)
    {
        const float warningRadius =
            std::max(currentBounds.size.x, currentBounds.size.y) * (0.35f + warningProgress * 0.35f);
        sf::CircleShape warningRing(warningRadius);
        warningRing.setOrigin({warningRadius, warningRadius});
        warningRing.setPosition(posedSprite.getPosition());
        warningRing.setFillColor(sf::Color::Transparent);
        warningRing.setOutlineThickness(2.0f + warningProgress * 2.0f);
        warningRing.setOutlineColor(sf::Color(255, 230, 150, static_cast<std::uint8_t>(120 + warningProgress * 100.0f)));
        target.draw(warningRing, states);
    }
}
