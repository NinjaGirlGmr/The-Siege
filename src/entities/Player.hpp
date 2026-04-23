#pragma once

#include "Entity.hpp"

#include <array>
#include <optional>
#include <string>
#include <unordered_map>

class Player : public Entity
{
public:
    static constexpr float PlayAreaWidth = 768.0f;
    static constexpr float LaneStep = 64.0f;
    static constexpr float LaneMoveDuration = 0.18f;
    static constexpr float DashMoveDuration = 0.11f;
    static constexpr float DefaultDashCooldown = 0.75f;

    Player();

    void handleInput(float deltaTime);
    void syncMovementTarget();
    void stepLeft();
    void stepRight();
    void dashLeft();
    void dashRight();
    void setShooting(bool shooting);

    void left(float deltaTime);
    void right(float deltaTime);
    void shoot();
    void updateVisualFeedback(float deltaTime);
    void triggerShotRecoil();
    void triggerDamageFlash();
    void applyDamageIfVulnerable(int damage);
    void reduceRemainingDashCooldown(float seconds);
    void setDashCooldownDuration(float dashCooldownDuration);
    float getDashCooldownDuration() const;

    bool isShooting() const;
    bool canDash() const;
    bool isInvulnerable() const;
    bool isPerformingDashAttack() const;
    float getDashCooldownProgress() const;
    float getDashVisualStrength() const;
    sf::Vector2f getProjectileSpawnPosition() const;
    std::optional<float> consumePendingProjectileLaunchDelay();

protected:
    void draw(sf::RenderTarget &target, sf::RenderStates states) const override;

private:
    static constexpr int SpriteSheetCellSize = 64;
    static constexpr float AnimationFramesPerSecond = 8.0f;
    static constexpr float VisualSpriteScaleMultiplier = 2.0f;
    static constexpr std::size_t SpellCastSpawnFrameIndex = 6;

    // Loads every sprite sheet used by the player and registers the default sheet on the entity.
    void loadPlayerSpriteSheets();
    // Shared file-loader used by both the idle and spell-cast sheets.
    bool loadSpriteSheetTexture(
        const std::array<const char *, 2> &spriteSheetPaths,
        sf::Texture &destinationTexture);
    // Builds named animation frame lists from sprite sheet grids.
    void initializeAnimationFrames();
    // Registers a state from a regular grid while optionally trimming empty cells at the end.
    void registerAnimationState(
        const std::string &animationState,
        int columnCount,
        int rowCount,
        std::size_t frameCount);
    // Swaps to a new named animation and rewinds to its first frame.
    void setAnimationState(const std::string &animationState);
    // Ensures the active state is drawing from the correct sprite sheet texture.
    void applyCurrentAnimationTexture();
    // Advances the active animation using elapsed real time.
    void advanceAnimation(float deltaTime);
    // Pushes the current frame rectangle onto the underlying SFML sprite.
    void applyCurrentAnimationFrame();
    // Queues the projectile spawn once the cast reaches the author-specified frame.
    void updateSpellCastProjectileSpawn();
    // Reports how long the current non-looping cast still has before returning to idle.
    float getCurrentAnimationSecondsRemaining() const;
    void beginLaneMove(float newTargetX, float moveDuration);
    float getLaneMovePosition(float progress) const;
    float getClampedX(float x) const;
    void clampToPlayArea();

    // Keep every loaded sprite sheet alive because the SFML sprite stores texture references.
    sf::Texture stanceSpriteSheetTexture_;
    sf::Texture spellBasicSpriteSheetTexture_;
    std::unordered_map<std::string, std::vector<sf::IntRect>> animationFramesByState_;
    std::unordered_map<std::string, const sf::Texture *> animationTextureByState_;
    std::unordered_map<std::string, bool> animationLoopingByState_;
    std::string currentAnimationState_ = "rest";
    std::size_t currentAnimationFrameIndex_ = 0;
    float animationFrameSecondsAccumulator_ = 0.0f;
    float moveStartX_ = 0.0f;
    float targetX_ = 0.0f;
    float previousX_ = 0.0f;
    float laneMoveElapsed_ = 0.0f;
    float currentMoveDuration_ = LaneMoveDuration;
    float laneMoveDirection_ = 0.0f;
    float dashCooldownDuration_ = DefaultDashCooldown;
    float dashCooldownRemaining_ = 0.0f;
    float dashVisualSecondsRemaining_ = 0.0f;
    float invulnerabilitySecondsRemaining_ = 0.0f;
    float pendingProjectileLaunchDelaySeconds_ = 0.0f;
    float shotRecoilSecondsRemaining_ = 0.0f;
    float damageFlashSecondsRemaining_ = 0.0f;
    bool movementTargetInitialized_ = false;
    bool laneMoving_ = false;
    bool shooting_ = false;
    bool activeAnimationFinished_ = false;
    bool spellProjectileSpawnedThisCast_ = false;
    bool pendingProjectileSpawnRequest_ = false;
};
