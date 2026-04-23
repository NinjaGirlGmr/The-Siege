#include "DashCooldownIndicator.hpp"

#include <algorithm>
#include <array>

DashCooldownIndicator::DashCooldownIndicator()
    : circle_(12.0f),
      ring_(17.0f)
{
    circle_.setOrigin({12.0f, 12.0f});
    circle_.setOutlineThickness(2.0f);
    circle_.setOutlineColor(sf::Color(255, 255, 255, 180));

    ring_.setOrigin({17.0f, 17.0f});
    ring_.setFillColor(sf::Color::Transparent);
    ring_.setOutlineThickness(3.0f);
    ring_.setOutlineColor(sf::Color(90, 210, 255, 0));
}

void DashCooldownIndicator::setCooldownProgress(float progress)
{
    const std::array<sf::Color, 6> dashColors = {
        sf::Color(70, 80, 110),
        sf::Color(70, 110, 150),
        sf::Color(60, 145, 190),
        sf::Color(70, 185, 220),
        sf::Color(90, 210, 255),
        sf::Color(150, 240, 255),
    };

    progress = std::clamp(progress, 0.0f, 1.0f);
    const std::size_t colorIndex = std::min<std::size_t>(
        static_cast<std::size_t>(progress * dashColors.size()),
        dashColors.size() - 1);
    circle_.setFillColor(dashColors[colorIndex]);
}

void DashCooldownIndicator::setDashEmphasis(float strength)
{
    strength = std::clamp(strength, 0.0f, 1.0f);
    const float circleRadius = 12.0f + 3.0f * strength;
    circle_.setRadius(circleRadius);
    circle_.setOrigin({circleRadius, circleRadius});
    circle_.setOutlineColor(sf::Color(
        220,
        245,
        255,
        static_cast<std::uint8_t>(180.0f + 60.0f * strength)));

    const float ringRadius = 17.0f + 8.0f * strength;
    ring_.setRadius(ringRadius);
    ring_.setOrigin({ringRadius, ringRadius});
    ring_.setOutlineColor(sf::Color(
        90,
        210,
        255,
        static_cast<std::uint8_t>(150.0f * strength)));
}

void DashCooldownIndicator::setPosition(sf::Vector2f position)
{
    circle_.setPosition(position);
    ring_.setPosition(position);
}

void DashCooldownIndicator::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
    target.draw(ring_, states);
    target.draw(circle_, states);
}
