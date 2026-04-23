#include "Player.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>

namespace
{
    constexpr float DashVisualDurationSeconds = 0.18f;
    constexpr float DashInvulnerabilityDurationSeconds = 0.14f;
    constexpr float ShotRecoilDurationSeconds = 0.07f;
    constexpr float PlayerDamageFlashDurationSeconds = 0.12f;
    constexpr std::array<const char *, 2> StanceSpriteSheetPaths = {
        "resources/sprites/Stance.png",
        "../resources/sprites/Stance.png",
    };
    constexpr std::array<const char *, 2> SpellBasicSpriteSheetPaths = {
        "resources/sprites/launchSpell.png",
        "../resources/sprites/launchSpell.png",
    };

    float lerp(float start, float end, float t)
    {
        return start + (end - start) * t;
    }

    float smoothStep(float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
}

Player::Player()
    : Entity(100, 5, 250.0f)
{
    // Player visuals are split across an idle sheet and a dedicated spell-cast sheet.
    // Load both up front so animation state changes are just texture swaps and rect updates.
    loadPlayerSpriteSheets();
    initializeAnimationFrames();
    setAnimationState("rest");
}

void Player::handleInput(float deltaTime)
{
    sf::Sprite *sprite = getSprite();
    if (!sprite)
    {
        return;
    }

    if (!movementTargetInitialized_)
    {
        syncMovementTarget();
    }

    // Idle loops forever, but the cast animation is a one-shot that owns projectile timing.
    setAnimationState(shooting_ ? "spellBasic" : "rest");
    advanceAnimation(deltaTime);
    updateSpellCastProjectileSpawn();

    if (shooting_ && activeAnimationFinished_)
    {
        // Once the one-shot cast finishes, return to the stance sheet on the next frame.
        shooting_ = false;
        spellProjectileSpawnedThisCast_ = false;
        setAnimationState("rest");
    }

    dashCooldownRemaining_ = std::max(0.0f, dashCooldownRemaining_ - deltaTime);
    previousX_ = sprite->getPosition().x;
    if (!laneMoving_)
    {
        return;
    }

    laneMoveElapsed_ = std::min(laneMoveElapsed_ + deltaTime, currentMoveDuration_);

    // Hold the start and end poses longer, then compress the travel into the middle of the move.
    const float progress = laneMoveElapsed_ / currentMoveDuration_;
    const float animatedX = getLaneMovePosition(progress);
    sprite->setPosition({animatedX, sprite->getPosition().y});

    if (progress >= 1.0f)
    {
        sprite->setPosition({targetX_, sprite->getPosition().y});
        laneMoving_ = false;
        laneMoveDirection_ = 0.0f;
    }
}

void Player::syncMovementTarget()
{
    // Reset every transient movement/combat timer so retries and level transitions
    // never carry a half-finished dash or flash into the next state.
    const sf::Vector2f position = getPosition();
    moveStartX_ = position.x;
    targetX_ = position.x;
    previousX_ = position.x;
    laneMoveElapsed_ = LaneMoveDuration;
    currentMoveDuration_ = LaneMoveDuration;
    laneMoveDirection_ = 0.0f;
    dashCooldownRemaining_ = 0.0f;
    dashVisualSecondsRemaining_ = 0.0f;
    invulnerabilitySecondsRemaining_ = 0.0f;
    damageFlashSecondsRemaining_ = 0.0f;
    movementTargetInitialized_ = true;
    laneMoving_ = false;
}

void Player::stepLeft()
{
    if (!movementTargetInitialized_)
    {
        syncMovementTarget();
    }

    beginLaneMove(getClampedX(targetX_ - LaneStep), LaneMoveDuration);
}

void Player::stepRight()
{
    if (!movementTargetInitialized_)
    {
        syncMovementTarget();
    }

    beginLaneMove(getClampedX(targetX_ + LaneStep), LaneMoveDuration);
}

void Player::dashLeft()
{
    if (!movementTargetInitialized_)
    {
        syncMovementTarget();
    }

    if (!canDash())
    {
        return;
    }

    const float dashTarget = getClampedX(targetX_ - (LaneStep * 2.0f));
    beginLaneMove(dashTarget, DashMoveDuration);
    if (laneMoving_)
    {
        // A successful dash starts three linked timers:
        // cooldown gating, extra visual punch, and a brief i-frame window.
        dashCooldownRemaining_ = dashCooldownDuration_;
        dashVisualSecondsRemaining_ = DashVisualDurationSeconds;
        invulnerabilitySecondsRemaining_ = DashInvulnerabilityDurationSeconds;
    }
}

void Player::dashRight()
{
    if (!movementTargetInitialized_)
    {
        syncMovementTarget();
    }

    if (!canDash())
    {
        return;
    }

    const float dashTarget = getClampedX(targetX_ + (LaneStep * 2.0f));
    beginLaneMove(dashTarget, DashMoveDuration);
    if (laneMoving_)
    {
        // Keep left/right dash setup identical so both directions feel the same.
        dashCooldownRemaining_ = dashCooldownDuration_;
        dashVisualSecondsRemaining_ = DashVisualDurationSeconds;
        invulnerabilitySecondsRemaining_ = DashInvulnerabilityDurationSeconds;
    }
}

void Player::setShooting(bool shooting)
{
    shooting_ = shooting;
    if (!shooting_)
    {
        spellProjectileSpawnedThisCast_ = false;
        pendingProjectileSpawnRequest_ = false;
        pendingProjectileLaunchDelaySeconds_ = 0.0f;
        activeAnimationFinished_ = false;
        setAnimationState("rest");
    }
}

void Player::left(float deltaTime)
{
    move({-getSpeed() * deltaTime, 0.0f});
    clampToPlayArea();
}

void Player::right(float deltaTime)
{
    move({getSpeed() * deltaTime, 0.0f});
    clampToPlayArea();
}

void Player::shoot()
{
    // Ignore repeat input while a cast is already in progress. The current animation
    // must finish so the projectile timing stays aligned with the authored frames.
    if (shooting_)
    {
        return;
    }

    shooting_ = true;
    spellProjectileSpawnedThisCast_ = false;
    pendingProjectileSpawnRequest_ = false;
    pendingProjectileLaunchDelaySeconds_ = 0.0f;
    activeAnimationFinished_ = false;
    setAnimationState("spellBasic");
}

void Player::updateVisualFeedback(float deltaTime)
{
    // All short-lived player feedback timers tick down here so the rest of the code
    // can query simple booleans/strength values without duplicating timer math.
    shotRecoilSecondsRemaining_ = std::max(0.0f, shotRecoilSecondsRemaining_ - deltaTime);
    dashVisualSecondsRemaining_ = std::max(0.0f, dashVisualSecondsRemaining_ - deltaTime);
    invulnerabilitySecondsRemaining_ =
        std::max(0.0f, invulnerabilitySecondsRemaining_ - deltaTime);
    damageFlashSecondsRemaining_ = std::max(0.0f, damageFlashSecondsRemaining_ - deltaTime);
}

void Player::triggerShotRecoil()
{
    shotRecoilSecondsRemaining_ = ShotRecoilDurationSeconds;
}

void Player::triggerDamageFlash()
{
    damageFlashSecondsRemaining_ = PlayerDamageFlashDurationSeconds;
}

void Player::applyDamageIfVulnerable(int damage)
{
    // Dash invulnerability is authoritative: if the player is in that window,
    // the projectile collision still resolves, but health does not change.
    if (isInvulnerable())
    {
        return;
    }

    takeDamage(damage);
    triggerDamageFlash();
}

void Player::reduceRemainingDashCooldown(float seconds)
{
    dashCooldownRemaining_ =
        std::max(0.0f, dashCooldownRemaining_ - std::max(0.0f, seconds));
}

void Player::setDashCooldownDuration(float dashCooldownDuration)
{
    dashCooldownDuration_ = std::max(0.1f, dashCooldownDuration);
    dashCooldownRemaining_ = std::min(dashCooldownRemaining_, dashCooldownDuration_);
}

float Player::getDashCooldownDuration() const
{
    return dashCooldownDuration_;
}

bool Player::isShooting() const
{
    return shooting_;
}

bool Player::canDash() const
{
    return dashCooldownRemaining_ <= 0.0f;
}

bool Player::isInvulnerable() const
{
    return invulnerabilitySecondsRemaining_ > 0.0f;
}

bool Player::isPerformingDashAttack() const
{
    // The dash attack window intentionally follows the visual dash timer,
    // not the full cooldown, so only the active burst can break enemies.
    return laneMoving_ && dashVisualSecondsRemaining_ > 0.0f;
}

float Player::getDashCooldownProgress() const
{
    return 1.0f - (dashCooldownRemaining_ / dashCooldownDuration_);
}

float Player::getDashVisualStrength() const
{
    if (DashVisualDurationSeconds <= 0.0f)
    {
        return 0.0f;
    }

    return std::clamp(
        dashVisualSecondsRemaining_ / DashVisualDurationSeconds,
        0.0f,
        1.0f);
}

sf::Vector2f Player::getProjectileSpawnPosition() const
{
    const sf::Sprite *sprite = getSprite();
    if (!sprite)
    {
        return {};
    }

    const sf::FloatRect bounds = sprite->getGlobalBounds();
    // The new cast sheet reads best if the shot appears near the top-center of the body,
    // with a small bias to the right to match the staff/hand position in frame 7.
    return {
        bounds.position.x + bounds.size.x * 0.56f,
        bounds.position.y + bounds.size.y * 0.08f,
    };
}

std::optional<float> Player::consumePendingProjectileLaunchDelay()
{
    if (!pendingProjectileSpawnRequest_)
    {
        return std::nullopt;
    }

    pendingProjectileSpawnRequest_ = false;
    return pendingProjectileLaunchDelaySeconds_;
}

void Player::clampToPlayArea()
{
    sf::Sprite *sprite = getSprite();
    if (!sprite)
    {
        return;
    }

    const sf::FloatRect bounds = sprite->getGlobalBounds();
    const float clampedX = getClampedX(bounds.position.x);

    sprite->setPosition({clampedX, bounds.position.y});
}

float Player::getClampedX(float x) const
{
    const sf::Sprite *sprite = getSprite();
    if (!sprite)
    {
        return x;
    }

    const sf::FloatRect bounds = sprite->getGlobalBounds();
    return std::clamp(x, 0.0f, PlayAreaWidth - bounds.size.x);
}

void Player::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
    const sf::Sprite *sprite = getSprite();
    if (!sprite)
    {
        return;
    }

    sf::Sprite posedSprite = *sprite;
    const float movementPoseStrength =
        laneMoving_ ? (0.4f + 0.6f * (1.0f - laneMoveElapsed_ / currentMoveDuration_)) : 0.0f;
    const float dashVisualStrength = getDashVisualStrength();
    const float shotRecoilStrength =
        ShotRecoilDurationSeconds <= 0.0f
            ? 0.0f
            : shotRecoilSecondsRemaining_ / ShotRecoilDurationSeconds;
    const float horizontalLean =
        laneMoveDirection_ * (4.0f * movementPoseStrength + 10.0f * dashVisualStrength);
    const float verticalKick = -3.0f * shotRecoilStrength;
    const sf::FloatRect localBounds = posedSprite.getLocalBounds();
    const sf::Vector2f spriteCenter = {
        localBounds.position.x + localBounds.size.x * 0.5f,
        localBounds.position.y + localBounds.size.y * 0.5f,
    };
    const sf::FloatRect currentBounds = sprite->getGlobalBounds();
    posedSprite.setOrigin(spriteCenter);
    posedSprite.setPosition({
        sprite->getPosition().x + currentBounds.size.x * 0.5f,
        sprite->getPosition().y + currentBounds.size.y * 0.5f + verticalKick,
    });
    // The game logic still uses the base 64x64 sprite bounds, but the art is drawn at
    // an integer 2x multiple here so the player reads larger without changing hitbox size.
    posedSprite.setScale({
        sprite->getScale().x * VisualSpriteScaleMultiplier *
            (1.0f + 0.05f * movementPoseStrength + 0.10f * dashVisualStrength - 0.03f * shotRecoilStrength),
        sprite->getScale().y * VisualSpriteScaleMultiplier *
            (1.0f - 0.04f * movementPoseStrength - 0.06f * dashVisualStrength + 0.07f * shotRecoilStrength),
    });
    posedSprite.setRotation(sf::degrees(horizontalLean));
    if (dashVisualStrength > 0.0f)
    {
        const sf::Color baseColor = posedSprite.getColor();
        posedSprite.setColor(sf::Color(
            static_cast<std::uint8_t>(std::min(255.0f, lerp(static_cast<float>(baseColor.r), 185.0f, dashVisualStrength * 0.65f))),
            static_cast<std::uint8_t>(std::min(255.0f, lerp(static_cast<float>(baseColor.g), 230.0f, dashVisualStrength * 0.7f))),
            static_cast<std::uint8_t>(std::min(255.0f, lerp(static_cast<float>(baseColor.b), 255.0f, dashVisualStrength * 0.9f))),
            baseColor.a));
    }
    if (damageFlashSecondsRemaining_ > 0.0f)
    {
        // Damage flash overrides the dash tint so being hit reads immediately.
        posedSprite.setColor(sf::Color(255, 245, 245));
    }

    const float velocityX = sprite->getPosition().x - previousX_;
    if (std::abs(velocityX) > 0.01f || dashVisualStrength > 0.0f)
    {
        const float direction =
            std::abs(velocityX) > 0.01f ? (velocityX > 0.0f ? 1.0f : -1.0f) : laneMoveDirection_;
        // Base the afterimage on actual frame-to-frame movement so quick taps stay tight.
        const float speed = std::abs(velocityX);
        const float trailSpacing =
            std::clamp(speed * (0.6f + 0.6f * dashVisualStrength), 4.0f, 18.0f);
        int ghostCount =
            speed < 5.0f ? 1 : speed < 10.0f ? 2
                                             : 3;
        if (dashVisualStrength > 0.0f)
        {
            ghostCount = std::max(ghostCount, 5);
        }
        const sf::Color baseColor = posedSprite.getColor();

        for (int index = 1; index <= ghostCount; ++index)
        {
            sf::Sprite ghost = posedSprite;
            ghost.move({-direction * trailSpacing * static_cast<float>(index), 0.0f});
            // Earlier ghosts stay brighter and bluer so the trail reads as a single burst
            // instead of a row of equally weighted copies.
            const float normalizedIndex = static_cast<float>(index) / static_cast<float>(ghostCount);
            const float dashTrailTintStrength =
                std::clamp((0.35f + 0.65f * dashVisualStrength) * (1.0f - normalizedIndex * 0.45f), 0.0f, 1.0f);
            const std::uint8_t alpha = static_cast<std::uint8_t>(
                std::clamp(
                    (dashVisualStrength > 0.0f ? 135.0f : 85.0f) * (1.0f - normalizedIndex * 0.75f),
                    18.0f,
                    160.0f));
            ghost.setColor(sf::Color(
                static_cast<std::uint8_t>(std::min(255.0f, lerp(static_cast<float>(baseColor.r), 90.0f, dashTrailTintStrength))),
                static_cast<std::uint8_t>(std::min(255.0f, lerp(static_cast<float>(baseColor.g), 190.0f, dashTrailTintStrength))),
                static_cast<std::uint8_t>(std::min(255.0f, lerp(static_cast<float>(baseColor.b), 255.0f, dashTrailTintStrength))),
                alpha));
            target.draw(ghost, states);
        }
    }

    target.draw(posedSprite, states);
}

void Player::loadPlayerSpriteSheets()
{
    const bool loadedStanceTexture =
        loadSpriteSheetTexture(StanceSpriteSheetPaths, stanceSpriteSheetTexture_);
    const bool loadedSpellTexture =
        loadSpriteSheetTexture(SpellBasicSpriteSheetPaths, spellBasicSpriteSheetTexture_);

    // The entity must always have some texture bound. Prefer the normal stance sheet,
    // but fall back to the spell sheet if only that one exists.
    if (loadedStanceTexture)
    {
        setTexture(stanceSpriteSheetTexture_);
        return;
    }

    if (loadedSpellTexture)
    {
        setTexture(spellBasicSpriteSheetTexture_);
        return;
    }

    // Keep the game playable if the file is missing by falling back to a plain square.
    const sf::Image fallbackImage({SpriteSheetCellSize, SpriteSheetCellSize}, sf::Color::White);
    if (stanceSpriteSheetTexture_.loadFromImage(fallbackImage))
    {
        stanceSpriteSheetTexture_.setSmooth(false);
        spellBasicSpriteSheetTexture_ = stanceSpriteSheetTexture_;
        setTexture(stanceSpriteSheetTexture_);
    }
}

bool Player::loadSpriteSheetTexture(
    const std::array<const char *, 2> &spriteSheetPaths,
    sf::Texture &destinationTexture)
{
    for (const char *spriteSheetPath : spriteSheetPaths)
    {
        // Try the runtime working directory first, then a parent-directory fallback
        // so launching from the build folder still finds the same asset.
        if (!std::filesystem::exists(spriteSheetPath))
        {
            continue;
        }

        if (!destinationTexture.loadFromFile(spriteSheetPath))
        {
            continue;
        }

        // Pixel art should stay crisp when scaled up for rendering.
        destinationTexture.setSmooth(false);
        return true;
    }

    return false;
}

void Player::initializeAnimationFrames()
{
    // Idle keeps using the original stance sheet. The spell cast is a 3x6 sheet with
    // only the first 16 cells populated, so the helper trims the empty tail cells.
    registerAnimationState("rest", 3, 3, 8);
    registerAnimationState("spellBasic", 3, 6, 16);

    animationTextureByState_["rest"] = &stanceSpriteSheetTexture_;
    animationTextureByState_["spellBasic"] = &spellBasicSpriteSheetTexture_;
    animationLoopingByState_["rest"] = true;
    animationLoopingByState_["spellBasic"] = false;
}

void Player::registerAnimationState(
    const std::string &animationState,
    int columnCount,
    int rowCount,
    std::size_t frameCount)
{
    std::vector<sf::IntRect> animationFrames;
    animationFrames.reserve(frameCount);

    std::size_t registeredFrameCount = 0;
    for (int rowIndex = 0; rowIndex < rowCount && registeredFrameCount < frameCount; ++rowIndex)
    {
        for (int columnIndex = 0; columnIndex < columnCount && registeredFrameCount < frameCount; ++columnIndex)
        {
            animationFrames.emplace_back(
                sf::Vector2i{columnIndex * SpriteSheetCellSize, rowIndex * SpriteSheetCellSize},
                sf::Vector2i{SpriteSheetCellSize, SpriteSheetCellSize});
            ++registeredFrameCount;
        }
    }

    animationFramesByState_[animationState] = std::move(animationFrames);
}

void Player::setAnimationState(const std::string &animationState)
{
    if (currentAnimationState_ == animationState)
    {
        return;
    }

    currentAnimationState_ = animationState;
    currentAnimationFrameIndex_ = 0;
    animationFrameSecondsAccumulator_ = 0.0f;
    activeAnimationFinished_ = false;
    // Apply immediately so state changes do not wait until the next frame boundary.
    applyCurrentAnimationTexture();
    applyCurrentAnimationFrame();
}

void Player::applyCurrentAnimationTexture()
{
    const auto textureIterator = animationTextureByState_.find(currentAnimationState_);
    if (textureIterator == animationTextureByState_.end() || textureIterator->second == nullptr)
    {
        return;
    }

    // Reset the rect when swapping sheets so SFML does not momentarily keep the old one.
    setTexture(*textureIterator->second, true);
}

void Player::advanceAnimation(float deltaTime)
{
    const auto animationFramesIterator = animationFramesByState_.find(currentAnimationState_);
    if (animationFramesIterator == animationFramesByState_.end() ||
        animationFramesIterator->second.empty())
    {
        return;
    }

    // Run the animation independently of render cadence so 8 fps stays stable even
    // while the game itself updates and draws at a different rate.
    const float secondsPerFrame = 1.0f / AnimationFramesPerSecond;
    animationFrameSecondsAccumulator_ += deltaTime;
    const auto animationLoopingIterator = animationLoopingByState_.find(currentAnimationState_);
    const bool animationLoops =
        animationLoopingIterator != animationLoopingByState_.end()
            ? animationLoopingIterator->second
            : true;

    while (animationFrameSecondsAccumulator_ >= secondsPerFrame)
    {
        animationFrameSecondsAccumulator_ -= secondsPerFrame;

        if (animationLoops)
        {
            currentAnimationFrameIndex_ =
                (currentAnimationFrameIndex_ + 1) % animationFramesIterator->second.size();
            continue;
        }

        if (currentAnimationFrameIndex_ + 1 < animationFramesIterator->second.size())
        {
            ++currentAnimationFrameIndex_;
            continue;
        }

        // One-shot animations stay on their final authored frame until its full frame
        // duration has been displayed, then they report completion back to handleInput().
        activeAnimationFinished_ = true;
        animationFrameSecondsAccumulator_ = 0.0f;
        break;
    }

    applyCurrentAnimationFrame();
}

void Player::applyCurrentAnimationFrame()
{
    const auto animationFramesIterator = animationFramesByState_.find(currentAnimationState_);
    if (animationFramesIterator == animationFramesByState_.end() ||
        animationFramesIterator->second.empty())
    {
        return;
    }

    // Texture rect changes the visible cell while Entity handles rescaling the sprite
    // back to the configured in-game size.
    setTextureRect(animationFramesIterator->second[currentAnimationFrameIndex_]);
}

void Player::updateSpellCastProjectileSpawn()
{
    if (!shooting_ || spellProjectileSpawnedThisCast_)
    {
        return;
    }

    if (currentAnimationState_ != "spellBasic" || currentAnimationFrameIndex_ < SpellCastSpawnFrameIndex)
    {
        return;
    }

    // Frame 7 is the author-chosen spawn moment. The projectile is created immediately
    // at that pose and launches right away from that same position.
    spellProjectileSpawnedThisCast_ = true;
    pendingProjectileSpawnRequest_ = true;
    pendingProjectileLaunchDelaySeconds_ = 0.0f;
}

float Player::getCurrentAnimationSecondsRemaining() const
{
    const auto animationFramesIterator = animationFramesByState_.find(currentAnimationState_);
    if (animationFramesIterator == animationFramesByState_.end() ||
        animationFramesIterator->second.empty() ||
        activeAnimationFinished_)
    {
        return 0.0f;
    }

    const float secondsPerFrame = 1.0f / AnimationFramesPerSecond;
    const float currentFrameSecondsRemaining =
        std::max(0.0f, secondsPerFrame - animationFrameSecondsAccumulator_);
    const std::size_t framesAfterCurrent =
        animationFramesIterator->second.size() - currentAnimationFrameIndex_ - 1;
    return currentFrameSecondsRemaining + secondsPerFrame * static_cast<float>(framesAfterCurrent);
}

void Player::beginLaneMove(float newTargetX, float moveDuration)
{
    sf::Sprite *sprite = getSprite();
    if (!sprite)
    {
        return;
    }

    const float currentX = sprite->getPosition().x;
    targetX_ = newTargetX;
    moveStartX_ = currentX;
    laneMoveElapsed_ = 0.0f;
    currentMoveDuration_ = std::max(0.001f, moveDuration);

    const float deltaX = targetX_ - currentX;
    laneMoveDirection_ = deltaX == 0.0f ? 0.0f : (deltaX > 0.0f ? 1.0f : -1.0f);
    laneMoving_ = std::abs(deltaX) > 0.01f;
}

float Player::getLaneMovePosition(float progress) const
{
    progress = std::clamp(progress, 0.0f, 1.0f);

    float travelProgress = 0.0f;
    if (progress >= 0.14f)
    {
        if (progress >= 0.82f)
        {
            travelProgress = 1.0f;
        }
        else
        {
            // Most of the distance is covered in the middle so the lane change reads as a snap, not a drift.
            travelProgress = smoothStep((progress - 0.14f) / 0.68f);
        }
    }

    return lerp(moveStartX_, targetX_, travelProgress);
}
