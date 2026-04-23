#include "Projectile.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>

namespace
{
    constexpr float AnimationFrameLength = 0.06f;
    constexpr float PlayerProjectileSpeed = 720.0f;
    constexpr float EnemyProjectileSpeed = 360.0f;
    constexpr float OffscreenTopLimit = -20.0f;
    constexpr float OffscreenBottomLimit = 1100.0f;
    constexpr int FireballFrameSize = 64;
    constexpr int FireballSpriteSheetColumns = 2;
    constexpr int FireballSpriteSheetRows = 3;
    constexpr int FireballAnimationFrameCount =
        FireballSpriteSheetColumns * FireballSpriteSheetRows;
    constexpr std::array<const char *, 2> FireballSpriteSheetPaths = {
        "resources/sprites/IcastFIREBALL.png",
        "../resources/sprites/IcastFIREBALL.png",
    };

sf::Vector2f normalizeOrFallback(sf::Vector2f vector, sf::Vector2f fallback)
{
    const float magnitudeSquared = vector.x * vector.x + vector.y * vector.y;
    if (magnitudeSquared <= 0.0001f)
    {
        return fallback;
    }

    const float inverseMagnitude = 1.0f / std::sqrt(magnitudeSquared);
    return {vector.x * inverseMagnitude, vector.y * inverseMagnitude};
}

sf::Vector2f rotateVectorDegrees(sf::Vector2f vector, float angleOffsetDegrees)
{
    const float radians = angleOffsetDegrees * 3.14159265f / 180.0f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return {
        vector.x * cosine - vector.y * sine,
        vector.x * sine + vector.y * cosine,
    };
}

sf::Color scaleColor(sf::Color color, float factor)
{
    return {
        static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(std::round(static_cast<float>(color.r) * factor)),
            0,
            255)),
        static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(std::round(static_cast<float>(color.g) * factor)),
            0,
            255)),
        static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(std::round(static_cast<float>(color.b) * factor)),
            0,
            255)),
        color.a,
    };
}

sf::Color mixColors(sf::Color first, sf::Color second, float secondAmount)
{
    const float clampedSecondAmount = std::clamp(secondAmount, 0.0f, 1.0f);
    const float firstAmount = 1.0f - clampedSecondAmount;
    return {
        static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(
                std::round(static_cast<float>(first.r) * firstAmount +
                           static_cast<float>(second.r) * clampedSecondAmount)),
            0,
            255)),
        static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(
                std::round(static_cast<float>(first.g) * firstAmount +
                           static_cast<float>(second.g) * clampedSecondAmount)),
            0,
            255)),
        static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(
                std::round(static_cast<float>(first.b) * firstAmount +
                           static_cast<float>(second.b) * clampedSecondAmount)),
            0,
            255)),
        first.a,
    };
}

void colorizeFireballImage(sf::Image &image, sf::Color coreColor, sf::Color edgeColor)
{
    const sf::Vector2u imageSize = image.getSize();
    for (unsigned int y = 0; y < imageSize.y; ++y)
    {
        for (unsigned int x = 0; x < imageSize.x; ++x)
        {
            const sf::Color sourceColor = image.getPixel({x, y});
            if (sourceColor.a == 0)
            {
                continue;
            }

            const float brightness =
                (0.299f * static_cast<float>(sourceColor.r) +
                 0.587f * static_cast<float>(sourceColor.g) +
                 0.114f * static_cast<float>(sourceColor.b)) /
                255.0f;
            const sf::Color hotColor = mixColors(edgeColor, coreColor, brightness);
            sf::Color colorizedColor = scaleColor(hotColor, 0.55f + brightness * 0.85f);
            colorizedColor.a = sourceColor.a;
            image.setPixel({x, y}, colorizedColor);
        }
    }
}

std::optional<sf::Texture> createTextureFromImage(const sf::Image &image)
{
    sf::Texture texture;
    if (!texture.loadFromImage(image))
    {
        return std::nullopt;
    }

    texture.setSmooth(false);
    return texture;
}

std::optional<sf::Texture> createFallbackFireballTexture(sf::Color color)
{
    const sf::Image fallbackImage(
        {FireballFrameSize, FireballFrameSize},
        sf::Color(color.r, color.g, color.b, 220));
    return createTextureFromImage(fallbackImage);
}

const sf::Texture *getFireballTexture(Projectile::VisualVariant visualVariant)
{
    static std::array<std::optional<sf::Texture>, 4> textures;
    static bool initialized = false;
    if (!initialized)
    {
        initialized = true;

        std::optional<sf::Image> sourceImage;
        for (const char *spriteSheetPath : FireballSpriteSheetPaths)
        {
            if (!std::filesystem::exists(spriteSheetPath))
            {
                continue;
            }

            sf::Image loadedImage;
            if (loadedImage.loadFromFile(spriteSheetPath))
            {
                sourceImage = loadedImage;
                break;
            }
        }

        if (sourceImage)
        {
            textures[static_cast<std::size_t>(Projectile::VisualVariant::Player)] =
                createTextureFromImage(*sourceImage);

            sf::Image enemyShooterImage = *sourceImage;
            colorizeFireballImage(
                enemyShooterImage,
                sf::Color(210, 255, 200),
                sf::Color(35, 210, 95));
            textures[static_cast<std::size_t>(Projectile::VisualVariant::EnemyShooter)] =
                createTextureFromImage(enemyShooterImage);

            sf::Image miniBossImage = *sourceImage;
            colorizeFireballImage(
                miniBossImage,
                sf::Color(255, 215, 230),
                sf::Color(185, 30, 130));
            textures[static_cast<std::size_t>(Projectile::VisualVariant::MiniBoss)] =
                createTextureFromImage(miniBossImage);

            sf::Image finalBossImage = *sourceImage;
            colorizeFireballImage(
                finalBossImage,
                sf::Color(255, 235, 170),
                sf::Color(255, 90, 20));
            textures[static_cast<std::size_t>(Projectile::VisualVariant::FinalBoss)] =
                createTextureFromImage(finalBossImage);
        }
        else
        {
            textures[static_cast<std::size_t>(Projectile::VisualVariant::Player)] =
                createFallbackFireballTexture(sf::Color::White);
            textures[static_cast<std::size_t>(Projectile::VisualVariant::EnemyShooter)] =
                createFallbackFireballTexture(sf::Color(35, 210, 95));
            textures[static_cast<std::size_t>(Projectile::VisualVariant::MiniBoss)] =
                createFallbackFireballTexture(sf::Color(185, 30, 130));
            textures[static_cast<std::size_t>(Projectile::VisualVariant::FinalBoss)] =
                createFallbackFireballTexture(sf::Color(255, 90, 20));
        }

        for (std::optional<sf::Texture> &texture : textures)
        {
            if (texture)
            {
                texture->setSmooth(false);
            }
        }
    }

    return textures[static_cast<std::size_t>(visualVariant)]
               ? &*textures[static_cast<std::size_t>(visualVariant)]
               : nullptr;
}
}

Projectile::Projectile(
    sf::Vector2f position,
    sf::Vector2f velocity,
    int damage,
    float radius,
    sf::Color startColor,
    float launchDelaySeconds,
    int remainingPierceCount,
    VisualVariant visualVariant)
    : body_(radius),
      velocity_(velocity),
      damage_(damage),
      remainingPierceCount_(std::max(0, remainingPierceCount)),
      launchDelaySecondsRemaining_(std::max(0.0f, launchDelaySeconds))
{
    // Origin is centered so the spawn position can be the exact middle of the shot.
    body_.setOrigin({radius, radius});
    body_.setPosition(position);
    body_.setFillColor(startColor);
    initializeSprite(startColor, visualVariant);
}

Projectile Projectile::createPlayerProjectile(
    sf::Vector2f position,
    float launchDelaySeconds)
{
    return Projectile(
        position,
        {0.0f, -PlayerProjectileSpeed},
        DefaultPlayerProjectileDamage,
        5.0f,
        sf::Color::White,
        launchDelaySeconds,
        0,
        VisualVariant::Player);
}

Projectile Projectile::createEnemyProjectile(sf::Vector2f position)
{
    return Projectile(
        position,
        {0.0f, EnemyProjectileSpeed},
        DefaultEnemyProjectileDamage,
        6.0f,
        sf::Color(255, 120, 120),
        0.0f,
        0,
        VisualVariant::EnemyShooter);
}

Projectile Projectile::createEnemyProjectileTowardTarget(
    sf::Vector2f position,
    sf::Vector2f targetPosition,
    float speedMultiplier,
    float angleOffsetDegrees,
    VisualVariant visualVariant)
{
    // Enemy shots snapshot the player's location at fire time. They are aimed, but
    // not homing, so movement after the launch still matters.
    const sf::Vector2f shotDirection =
        rotateVectorDegrees(
            normalizeOrFallback(targetPosition - position, {0.0f, 1.0f}),
            angleOffsetDegrees);
    return Projectile(
        position,
        shotDirection * (EnemyProjectileSpeed * std::max(0.1f, speedMultiplier)),
        DefaultEnemyProjectileDamage,
        6.0f,
        sf::Color(255, 120, 120),
        0.0f,
        0,
        visualVariant);
}

void Projectile::update(float deltaTime)
{
    animationTimer_ += deltaTime;
    updateSpriteFrame();

    // Player spell shots can appear at the hand/cast point first, then begin traveling once
    // the remainder of the cast animation has elapsed.
    float movementDeltaTime = deltaTime;
    if (launchDelaySecondsRemaining_ > 0.0f)
    {
        if (deltaTime <= launchDelaySecondsRemaining_)
        {
            launchDelaySecondsRemaining_ -= deltaTime;
            return;
        }

        movementDeltaTime = deltaTime - launchDelaySecondsRemaining_;
        launchDelaySecondsRemaining_ = 0.0f;
    }

    body_.move(velocity_ * movementDeltaTime);
    syncSpriteTransform();
}

void Projectile::destroy()
{
    active_ = false;
}

bool Projectile::isActive() const
{
    return active_;
}

bool Projectile::isOffscreen() const
{
    const sf::FloatRect bounds = body_.getGlobalBounds();
    return bounds.position.y + bounds.size.y < OffscreenTopLimit ||
           bounds.position.y > OffscreenBottomLimit;
}

int Projectile::getDamage() const
{
    return damage_;
}

sf::FloatRect Projectile::getBounds() const
{
    return body_.getGlobalBounds();
}

bool Projectile::consumePierceCharge()
{
    if (remainingPierceCount_ <= 0)
    {
        return false;
    }

    --remainingPierceCount_;
    return true;
}

void Projectile::nudgeForwardAfterPierce(float distance)
{
    const sf::Vector2f direction = normalizeOrFallback(velocity_, {0.0f, -1.0f});
    body_.move(direction * std::max(0.0f, distance));
    syncSpriteTransform();
}

void Projectile::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
    if (sprite_)
    {
        target.draw(*sprite_, states);
        return;
    }

    target.draw(body_, states);
}

void Projectile::initializeSprite(sf::Color fallbackColor, VisualVariant visualVariant)
{
    const sf::Texture *texture = getFireballTexture(visualVariant);
    if (!texture)
    {
        body_.setFillColor(fallbackColor);
        return;
    }

    sprite_.emplace(*texture);
    sprite_->setColor(sf::Color::White);
    updateSpriteFrame();
    const float visualSize = body_.getRadius() * 4.8f;
    sprite_->setScale({visualSize / static_cast<float>(FireballFrameSize),
                       visualSize / static_cast<float>(FireballFrameSize)});
    sprite_->setOrigin({FireballFrameSize * 0.5f, FireballFrameSize * 0.5f});
    syncSpriteTransform();
}

void Projectile::updateSpriteFrame()
{
    if (!sprite_)
    {
        return;
    }

    const int frameIndex =
        static_cast<int>(animationTimer_ / AnimationFrameLength) % FireballAnimationFrameCount;
    const int frameColumn = frameIndex % FireballSpriteSheetColumns;
    const int frameRow = frameIndex / FireballSpriteSheetColumns;
    sprite_->setTextureRect(sf::IntRect(
        {frameColumn * FireballFrameSize, frameRow * FireballFrameSize},
        {FireballFrameSize, FireballFrameSize}));
}

void Projectile::syncSpriteTransform()
{
    if (!sprite_)
    {
        return;
    }

    sprite_->setPosition(body_.getPosition());
    const sf::Vector2f direction = normalizeOrFallback(velocity_, {0.0f, -1.0f});
    const float rotationDegrees =
        std::atan2(direction.y, direction.x) * 180.0f / 3.14159265f + 90.0f;
    sprite_->setRotation(sf::degrees(rotationDegrees));
}
