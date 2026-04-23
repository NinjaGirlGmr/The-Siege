#pragma once

#include <SFML/Graphics.hpp>

class DashCooldownIndicator : public sf::Drawable
{
public:
    DashCooldownIndicator();

    void setCooldownProgress(float progress);
    void setDashEmphasis(float strength);
    void setPosition(sf::Vector2f position);

private:
    void draw(sf::RenderTarget &target, sf::RenderStates states) const override;

    sf::CircleShape circle_;
    sf::CircleShape ring_;
};
