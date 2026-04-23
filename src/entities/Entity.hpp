#pragma once

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <optional>

class Entity : public sf::Drawable
{
public:
    // Shared default sprite size for entities that use 64x64 art.
    static constexpr float DefaultSpriteSize = 64.0f;

    Entity(
        int maxHealth = 100,
        int defense = 0,
        float speed = 100.0f,
        sf::Vector2f spriteSize = {DefaultSpriteSize, DefaultSpriteSize});

    void setMaxHealth(int maxHealth);
    int getMaxHealth() const;

    void setHealth(int health);
    int getHealth() const;

    void heal(int amount);
    void takeDamage(int damage);
    bool isAlive() const;

    void setDefense(int defense);
    int getDefense() const;

    void setSpeed(float speed);
    float getSpeed() const;

    void setSpriteSize(sf::Vector2f spriteSize);
    sf::Vector2f getSpriteSize() const;

    void setTexture(const sf::Texture &texture, bool resetRect = true);
    void setTextureRect(const sf::IntRect &textureRect);
    sf::Sprite *getSprite();
    const sf::Sprite *getSprite() const;

    void setPosition(sf::Vector2f position);
    sf::Vector2f getPosition() const;
    void move(sf::Vector2f offset);

protected:
    void draw(sf::RenderTarget &target, sf::RenderStates states) const override;

private:
    // Recalculates sprite scale so textures fit the configured entity size.
    void updateSpriteScale();

    int maxHealth_;
    int health_;
    int defense_;
    float speed_;
    sf::Vector2f spriteSize_;
    // SFML 3 sprites require a texture at construction, so this starts empty.
    std::optional<sf::Sprite> sprite_;
};
