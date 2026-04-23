#pragma once

#include <SFML/Graphics.hpp>

#include <random>
#include <string>
#include <vector>

class Enemy;
class Player;

namespace UpgradeShop
{
    // The level stat bundle is now shared with the shop so performance-aware offers
    // can be generated from the same data the gameplay loop already records.
    struct LevelStats
    {
        int shotsFired = 0;
        int shotsHit = 0;
        int dashKills = 0;
        int nearMisses = 0;
        int damageTaken = 0;
        int scoreEarned = 0;
        int essenceEarned = 0;
        int enemiesDefeated = 0;
        int maxCombo = 0;
        int currentCombo = 0;
        float comboGraceSecondsRemaining = 0.0f;
        float elapsedSeconds = 0.0f;
        bool tookDamage = false;
    };

    enum class OfferRarity
    {
        Common,
        Rare,
        Legendary,
        Cursed,
    };

    enum class OfferType
    {
        VitalityStitch,
        SharpenSpell,
        FleetStep,
        PiercingRune,
        BlinkReserve,
        ExecutionSigil,
        ComboLoom,
        EchoWeave,
        HeavyVeil,
        GlassFocus,
        SniperSigil,
        SovereignVolley,
        PhoenixCore,
        CelestialLattice,
        BloodPrice,
        BorrowedTime,
        AegisThread,
        LifebloomTalisman,
        OverloadGlyph,
        AdrenalineStitch,
        RicochetRune,
        VampireBolt,
        ExecutionBloom,
        BossGrudge,
        EmotionalSupportRock,
        QuestionableSmoothie,
        TaxEvasion,
        SpiteButton,
        PanicEngine,
        GremlinContract,
        FragileCrown,
        SpitefulCore,
    };

    struct PurchasedUpgradeRecord
    {
        OfferType offerType = OfferType::VitalityStitch;
        int essencePaid = 0;
    };

    struct PlayerUpgradeState
    {
        int bonusProjectileDamage = 0;
        int bonusMaxHealth = 0;
        int bonusDefense = 0;
        int projectilePierceCount = 0;
        int bonusDamageAgainstShielded = 0;
        int bonusDamageAgainstLowHealth = 0;
        int comboThresholdForDamageBonus = 0;
        int comboBonusProjectileDamage = 0;
        int burstShotInterval = 0;
        int burstShotBonusDamage = 0;
        int ricochetHitInterval = 0;
        int ricochetDamage = 0;
        int projectileHitsSinceRicochet = 0;
        int vampireHealOnKill = 0;
        int executionBloomDamage = 0;
        int bossBonusDamage = 0;
        int firstPurchaseDiscount = 0;
        int flatShopDiscount = 0;
        int spiteButtonBonusDamage = 0;
        int fragileCrownBonusDamage = 0;
        int spitefulCoreDamage = 0;
        int dashChargedShotBonus = 0;
        int pendingNextShotBonusDamage = 0;
        int echoShotInterval = 0;
        int echoShotCount = 0;
        int bonusHealOnPurchase = 0;
        int totalShotsFiredThisRun = 0;
        float dashCooldownMultiplierOffset = 0.0f;
        float panicDashCooldownMultiplierOffset = 0.0f;
        bool forceCursedOfferInShop = false;
        std::vector<PurchasedUpgradeRecord> purchaseHistory;
    };

    struct ShopOffer
    {
        OfferType offerType = OfferType::VitalityStitch;
        OfferRarity rarity = OfferRarity::Common;
        std::string label;
        std::string description;
        std::string recommendationText;
        std::string spriteKeyword;
        int essenceCost = 0;
        bool isRecommended = false;
    };

    struct ShopState
    {
        std::vector<ShopOffer> currentOffers;
        bool purchaseMade = false;
        bool tradeInUsed = false;
        int firstPurchaseDiscount = 0;
        int rerollCount = 0;
        std::string statusText;
    };

    struct FiredShotPlan
    {
        int projectileDamage = 0;
        int projectilePierceCount = 0;
        int echoProjectileCount = 0;
    };

    // The overlay bounds live with the shop module because the shop now owns the card layout.
    constexpr float OverlayPanelWidth = 620.0f;
    constexpr float OverlayPanelHeight = 570.0f;
    constexpr float OverlayPanelX = 74.0f;
    constexpr float OverlayPanelY = 190.0f;

    void resetLevelStats(LevelStats &levelStats);
    int getAccuracyPercentage(const LevelStats &levelStats);

    std::vector<ShopOffer> buildShopOffersForLevel(
        int currentLevelNumber,
        const LevelStats &completedLevelStats,
        const PlayerUpgradeState &upgradeState,
        int playerEssence,
        std::mt19937 &randomNumberGenerator);

    void applyPlayerUpgradeStats(Player &player, const PlayerUpgradeState &upgradeState);

    bool tryPurchaseShopOffer(
        const ShopOffer &shopOffer,
        Player &player,
        int &playerEssence,
        PlayerUpgradeState &upgradeState,
        std::string &statusMessage);

    bool tryTradeInLatestUpgrade(
        Player &player,
        int &playerEssence,
        PlayerUpgradeState &upgradeState,
        std::string &statusMessage);

    bool tryRerollShopOffers(
        int currentLevelNumber,
        const LevelStats &completedLevelStats,
        int &playerEssence,
        const PlayerUpgradeState &upgradeState,
        ShopState &shopState,
        std::mt19937 &randomNumberGenerator,
        std::string &statusMessage);

    void applySkipRewards(
        Player &player,
        int &playerEssence,
        std::string &statusMessage);

    void notifyPlayerDashPerformed(PlayerUpgradeState &upgradeState);
    FiredShotPlan consumeNextShotPlan(
        const LevelStats &currentLevelStats,
        PlayerUpgradeState &upgradeState);
    int calculateProjectileDamageBonusOnHit(
        const Enemy &enemy,
        const PlayerUpgradeState &upgradeState);

    std::string buildShopMessage(const ShopState &shopState, int playerEssence);
    std::string getOfferRarityLabel(OfferRarity offerRarity);
    sf::Color getOfferAccentColor(OfferRarity offerRarity);

    sf::FloatRect getPanelBounds();
    sf::FloatRect getContinueButtonBounds();
    sf::FloatRect getCardBounds(std::size_t offerIndex);
    sf::FloatRect getRerollButtonBounds();
    sf::FloatRect getTradeInButtonBounds();

    void drawShopOverlay(
        sf::RenderWindow &window,
        const sf::Font &font,
        const ShopState &shopState,
        int playerEssence);
}
