#include "Entity.hpp"

Entity::Entity(int maxHealth, int defense, float speed, sf::Vector2f spriteSize)
    : maxHealth_(std::max(1, maxHealth)),
      health_(std::max(1, maxHealth)),
      defense_(std::max(0, defense)),
      speed_(std::max(0.0f, speed)),
      spriteSize_(spriteSize)
{
}

void Entity::setMaxHealth(int maxHealth)
{
    // Max health should never drop below 1, and current health cannot exceed it.
    maxHealth_ = std::max(1, maxHealth);
    health_ = std::min(health_, maxHealth_);
}

int Entity::getMaxHealth() const
{
    return maxHealth_;
}

void Entity::setHealth(int health)
{
    // Keep health in a valid range between dead and full health.
    health_ = std::clamp(health, 0, maxHealth_);
}

int Entity::getHealth() const
{
    return health_;
}

void Entity::heal(int amount)
{
    setHealth(health_ + std::max(0, amount));
}

void Entity::takeDamage(int damage)
{
    // Defense reduces incoming damage but never turns it into healing.
    const int reducedDamage = std::max(0, damage - defense_);
    setHealth(health_ - reducedDamage);
}

bool Entity::isAlive() const
{
    return health_ > 0;
}

void Entity::setDefense(int defense)
{
    defense_ = std::max(0, defense);
}

int Entity::getDefense() const
{
    return defense_;
}

void Entity::setSpeed(float speed)
{
    speed_ = std::max(0.0f, speed);
}

float Entity::getSpeed() const
{
    return speed_;
}

void Entity::setSpriteSize(sf::Vector2f spriteSize)
{
    spriteSize_ = spriteSize;
    updateSpriteScale();
}

sf::Vector2f Entity::getSpriteSize() const
{
    return spriteSize_;
}

void Entity::setTexture(const sf::Texture &texture, bool resetRect)
{
    // Create the sprite on first texture assignment, then reuse it after that.
    if (sprite_.has_value())
    {
        sprite_->setTexture(texture, resetRect);
    }
    else
    {
        sprite_.emplace(texture);
    }

    updateSpriteScale();
}

void Entity::setTextureRect(const sf::IntRect &textureRect)
{
    if (!sprite_)
    {
        return;
    }

    sprite_->setTextureRect(textureRect);
    updateSpriteScale();
}

sf::Sprite *Entity::getSprite()
{
    return sprite_ ? &*sprite_ : nullptr;
}

const sf::Sprite *Entity::getSprite() const
{
    return sprite_ ? &*sprite_ : nullptr;
}

void Entity::setPosition(sf::Vector2f position)
{
    if (sprite_)
    {
        sprite_->setPosition(position);
    }
}

sf::Vector2f Entity::getPosition() const
{
    return sprite_ ? sprite_->getPosition() : sf::Vector2f();
}

void Entity::move(sf::Vector2f offset)
{
    if (sprite_)
    {
        sprite_->move(offset);
    }
}

void Entity::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
    if (sprite_)
    {
        target.draw(*sprite_, states);
    }
}

void Entity::updateSpriteScale()
{
    if (!sprite_)
    {
        return;
    }

    // Avoid invalid scaling when no texture area has been defined yet.
    const sf::FloatRect bounds = sprite_->getLocalBounds();
    if (bounds.size.x <= 0.0f || bounds.size.y <= 0.0f)
    {
        return;
    }

    // Scale the texture to match the entity's configured in-game size.
    sprite_->setScale({
        spriteSize_.x / bounds.size.x,
        spriteSize_.y / bounds.size.y,
    });
}
