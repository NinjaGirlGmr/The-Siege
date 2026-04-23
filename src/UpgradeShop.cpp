#include "UpgradeShop.hpp"

#include "entities/Enemy.hpp"
#include "entities/Player.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <optional>
#include <set>

namespace
{
    using UpgradeShop::FiredShotPlan;
    using UpgradeShop::LevelStats;
    using UpgradeShop::OfferRarity;
    using UpgradeShop::OfferType;
    using UpgradeShop::PlayerUpgradeState;
    using UpgradeShop::PurchasedUpgradeRecord;
    using UpgradeShop::ShopOffer;
    using UpgradeShop::ShopState;

    struct OfferDefinition
    {
        OfferType offerType;
        OfferRarity rarity;
        const char *label;
        const char *description;
        const char *spriteKeyword;
        int baseCost;
        int levelCostStep;
    };

    // This table is the source of truth for the current shop roster.
    // The rest of the code picks from here, recommends from here, and renders icons from here.
    constexpr std::array<OfferDefinition, 32> OfferDefinitions = {{
        {
            OfferType::VitalityStitch,
            OfferRarity::Common,
            "Vitality Stitch",
            "+20 max HP and heal 20 immediately.",
            "heart",
            6,
            1,
        },
        {
            OfferType::SharpenSpell,
            OfferRarity::Common,
            "Sharpen Spell",
            "+4 projectile damage on every shot.",
            "blade",
            7,
            1,
        },
        {
            OfferType::FleetStep,
            OfferRarity::Common,
            "Fleet Step",
            "Dash cooldown becomes 12% faster.",
            "dash",
            6,
            1,
        },
        {
            OfferType::PiercingRune,
            OfferRarity::Common,
            "Piercing Rune",
            "Shots can continue through 1 extra enemy.",
            "pierce",
            7,
            1,
        },
        {
            OfferType::BlinkReserve,
            OfferRarity::Common,
            "Blink Reserve",
            "After a dash, your next shot gains +12 damage.",
            "blink",
            8,
            1,
        },
        {
            OfferType::ExecutionSigil,
            OfferRarity::Common,
            "Execution Sigil",
            "+6 damage to shielded and weakened enemies.",
            "execute",
            8,
            1,
        },
        {
            OfferType::ComboLoom,
            OfferRarity::Common,
            "Combo Loom",
            "At 5 combo or higher, shots gain +6 damage.",
            "combo",
            8,
            1,
        },
        {
            OfferType::EchoWeave,
            OfferRarity::Rare,
            "Echo Weave",
            "Every 4th shot fires an extra echo bolt.",
            "echo",
            11,
            2,
        },
        {
            OfferType::HeavyVeil,
            OfferRarity::Rare,
            "Heavy Veil",
            "+40 max HP, but dash cooldown becomes 18% slower.",
            "armor",
            10,
            2,
        },
        {
            OfferType::GlassFocus,
            OfferRarity::Rare,
            "Glass Focus",
            "+10 projectile damage, but -20 max HP.",
            "glass",
            11,
            2,
        },
        {
            OfferType::SniperSigil,
            OfferRarity::Rare,
            "Sniper Sigil",
            "+8 projectile damage for precision-focused runs.",
            "precision",
            10,
            2,
        },
        {
            OfferType::SovereignVolley,
            OfferRarity::Legendary,
            "Sovereign Volley",
            "+12 projectile damage and every 3rd shot echoes.",
            "crown",
            16,
            3,
        },
        {
            OfferType::PhoenixCore,
            OfferRarity::Legendary,
            "Phoenix Core",
            "+30 max HP and heal 30 immediately.",
            "phoenix",
            15,
            3,
        },
        {
            OfferType::CelestialLattice,
            OfferRarity::Legendary,
            "Celestial Lattice",
            "Shots pierce +1 more enemy and gain +8 execute damage.",
            "star",
            17,
            3,
        },
        {
            OfferType::BloodPrice,
            OfferRarity::Cursed,
            "Blood Price",
            "+14 projectile damage, but lose 18 HP now.",
            "curse",
            9,
            2,
        },
        {
            OfferType::BorrowedTime,
            OfferRarity::Cursed,
            "Borrowed Time",
            "Dash cooldown becomes 22% faster, but -15 max HP.",
            "hourglass",
            9,
            2,
        },
        {
            OfferType::AegisThread,
            OfferRarity::Common,
            "Aegis Thread",
            "+2 defense against incoming damage.",
            "armor",
            7,
            1,
        },
        {
            OfferType::LifebloomTalisman,
            OfferRarity::Rare,
            "Lifebloom Talisman",
            "This and future purchases heal 8 HP.",
            "heart",
            10,
            2,
        },
        {
            OfferType::OverloadGlyph,
            OfferRarity::Rare,
            "Overload Glyph",
            "Every 5th shot gains +25 damage.",
            "star",
            12,
            2,
        },
        {
            OfferType::AdrenalineStitch,
            OfferRarity::Cursed,
            "Adrenaline Stitch",
            "+5 damage and faster dash, but -12 max HP.",
            "curse",
            8,
            2,
        },
        {
            OfferType::RicochetRune,
            OfferRarity::Rare,
            "Ricochet Rune",
            "Every 4th projectile hit jumps +8 damage to another enemy.",
            "pierce",
            12,
            2,
        },
        {
            OfferType::VampireBolt,
            OfferRarity::Rare,
            "Vampire Bolt",
            "Projectile kills heal 2 HP.",
            "heart",
            11,
            2,
        },
        {
            OfferType::ExecutionBloom,
            OfferRarity::Legendary,
            "Execution Bloom",
            "Low-health projectile kills burst for 10 damage nearby.",
            "execute",
            16,
            3,
        },
        {
            OfferType::BossGrudge,
            OfferRarity::Rare,
            "Boss Grudge",
            "+12 projectile damage against bosses only.",
            "precision",
            12,
            2,
        },
        {
            OfferType::EmotionalSupportRock,
            OfferRarity::Common,
            "Emotional Support Rock",
            "+1 defense. It believes in you. Silently.",
            "armor",
            5,
            1,
        },
        {
            OfferType::QuestionableSmoothie,
            OfferRarity::Cursed,
            "Questionable Smoothie",
            "Heal 35 now, but lose 10 max HP.",
            "glass",
            6,
            1,
        },
        {
            OfferType::TaxEvasion,
            OfferRarity::Rare,
            "Tax Evasion",
            "First purchase each shop is 3 essence cheaper. The IRS has not reached this realm.",
            "coupon",
            11,
            2,
        },
        {
            OfferType::SpiteButton,
            OfferRarity::Common,
            "Spite Button",
            "After taking damage, your next shot gains +18 damage.",
            "blink",
            8,
            1,
        },
        {
            OfferType::PanicEngine,
            OfferRarity::Rare,
            "Panic Engine",
            "Dash cooldown gets faster while below half health.",
            "dash",
            10,
            2,
        },
        {
            OfferType::GremlinContract,
            OfferRarity::Cursed,
            "Gremlin Contract",
            "All shops are 2 essence cheaper, but always include a cursed offer.",
            "curse",
            9,
            2,
        },
        {
            OfferType::FragileCrown,
            OfferRarity::Legendary,
            "Fragile Crown",
            "+24 shot damage after no-damage levels. Useless after messy ones.",
            "crown",
            15,
            3,
        },
        {
            OfferType::SpitefulCore,
            OfferRarity::Legendary,
            "Spiteful Core",
            "Taking damage shocks all enemies for 6 damage.",
            "star",
            17,
            3,
        },
    }};

    constexpr float CommonWeight = 1.0f;
    constexpr float RareWeight = 0.8f;
    constexpr float LegendaryWeight = 0.22f;
    constexpr float CursedWeight = 0.45f;

    const OfferDefinition *findDefinition(OfferType offerType)
    {
        for (const OfferDefinition &offerDefinition : OfferDefinitions)
        {
            if (offerDefinition.offerType == offerType)
            {
                return &offerDefinition;
            }
        }

        return nullptr;
    }

    int getLevelScaledCost(const OfferDefinition &offerDefinition, int currentLevelNumber)
    {
        return offerDefinition.baseCost +
               std::max(0, currentLevelNumber - 1) / 2 * offerDefinition.levelCostStep;
    }

    bool playerOwnsProjectilePlanUpgrade(const PlayerUpgradeState &upgradeState)
    {
        return upgradeState.projectilePierceCount > 0 ||
               upgradeState.echoShotCount > 0 ||
               upgradeState.dashChargedShotBonus > 0;
    }

    bool isOfferUnlockedByPerformance(
        OfferType offerType,
        const LevelStats &completedLevelStats)
    {
        const int accuracyPercentage = UpgradeShop::getAccuracyPercentage(completedLevelStats);
        const bool noDamageClear =
            completedLevelStats.enemiesDefeated > 0 &&
            completedLevelStats.damageTaken == 0;

        switch (offerType)
        {
            case OfferType::SniperSigil:
                return accuracyPercentage >= 70 || noDamageClear;
            case OfferType::SovereignVolley:
                return accuracyPercentage >= 80 || completedLevelStats.maxCombo >= 7;
            case OfferType::PhoenixCore:
                return noDamageClear || completedLevelStats.damageTaken >= 20;
            case OfferType::CelestialLattice:
                return completedLevelStats.maxCombo >= 7 ||
                       completedLevelStats.enemiesDefeated >= 8;
            case OfferType::BlinkReserve:
                return completedLevelStats.dashKills >= 2 ||
                       completedLevelStats.nearMisses >= 2;
            case OfferType::ComboLoom:
                return completedLevelStats.maxCombo >= 5;
            default:
                return true;
        }
    }

    float getBaseOfferWeight(const OfferDefinition &offerDefinition)
    {
        switch (offerDefinition.rarity)
        {
            case OfferRarity::Common:
                return CommonWeight;
            case OfferRarity::Rare:
                return RareWeight;
            case OfferRarity::Legendary:
                return LegendaryWeight;
            case OfferRarity::Cursed:
                return CursedWeight;
        }

        return CommonWeight;
    }

    float getSynergyWeightBonus(
        OfferType offerType,
        const PlayerUpgradeState &upgradeState)
    {
        switch (offerType)
        {
            case OfferType::PiercingRune:
                return upgradeState.bonusProjectileDamage >= 8 ? 0.5f : 0.0f;
            case OfferType::SovereignVolley:
                return upgradeState.echoShotCount > 0 ||
                               upgradeState.bonusProjectileDamage >= 8
                           ? 1.1f
                           : 0.0f;
            case OfferType::PhoenixCore:
                return upgradeState.bonusMaxHealth >= 20 ? 0.8f : 0.0f;
            case OfferType::CelestialLattice:
                return upgradeState.projectilePierceCount > 0 ? 1.0f : 0.0f;
            case OfferType::BlinkReserve:
                return upgradeState.dashCooldownMultiplierOffset < 0.0f ? 0.7f : 0.0f;
            case OfferType::ExecutionSigil:
                return upgradeState.projectilePierceCount > 0 ? 0.8f : 0.0f;
            case OfferType::ComboLoom:
                return playerOwnsProjectilePlanUpgrade(upgradeState) ? 0.6f : 0.0f;
            case OfferType::EchoWeave:
                return upgradeState.bonusProjectileDamage >= 4 ? 0.9f : 0.0f;
            case OfferType::HeavyVeil:
                return upgradeState.dashChargedShotBonus > 0 ? 0.25f : 0.0f;
            case OfferType::GlassFocus:
                return upgradeState.bonusProjectileDamage >= 4 ? 0.7f : 0.0f;
            case OfferType::BloodPrice:
                return upgradeState.bonusMaxHealth >= 20 ? 0.4f : 0.0f;
            case OfferType::AegisThread:
                return upgradeState.bonusMaxHealth >= 20 ? 0.6f : 0.0f;
            case OfferType::LifebloomTalisman:
                return upgradeState.purchaseHistory.size() <= 2 ? 0.5f : 0.2f;
            case OfferType::OverloadGlyph:
                return upgradeState.echoShotCount > 0 ||
                               upgradeState.projectilePierceCount > 0
                           ? 0.9f
                           : 0.0f;
            case OfferType::AdrenalineStitch:
                return upgradeState.dashCooldownMultiplierOffset < 0.0f ? 0.7f : 0.0f;
            default:
                return 0.0f;
        }
    }

    float getPerformanceWeightBonus(
        OfferType offerType,
        const LevelStats &completedLevelStats)
    {
        const int accuracyPercentage = UpgradeShop::getAccuracyPercentage(completedLevelStats);
        const bool noDamageClear =
            completedLevelStats.enemiesDefeated > 0 &&
            completedLevelStats.damageTaken == 0;

        switch (offerType)
        {
            case OfferType::SniperSigil:
                return accuracyPercentage >= 70 ? 2.0f : (noDamageClear ? 1.5f : 0.0f);
            case OfferType::SovereignVolley:
                return accuracyPercentage >= 80 ? 0.9f : 0.0f;
            case OfferType::PhoenixCore:
                return completedLevelStats.damageTaken >= 20 ? 1.0f : 0.0f;
            case OfferType::CelestialLattice:
                return completedLevelStats.maxCombo >= 7 ? 0.9f : 0.0f;
            case OfferType::BlinkReserve:
                return completedLevelStats.dashKills >= 2 ||
                               completedLevelStats.nearMisses >= 2
                           ? 1.6f
                           : 0.0f;
            case OfferType::ComboLoom:
                return completedLevelStats.maxCombo >= 5 ? 1.7f : 0.0f;
            case OfferType::FleetStep:
                return completedLevelStats.nearMisses >= 2 ? 0.7f : 0.0f;
            case OfferType::GlassFocus:
                return noDamageClear ? 0.8f : 0.0f;
            case OfferType::AegisThread:
                return completedLevelStats.damageTaken >= 12 ? 1.2f : 0.0f;
            case OfferType::LifebloomTalisman:
                return completedLevelStats.damageTaken >= 20 ? 1.4f : 0.0f;
            case OfferType::OverloadGlyph:
                return completedLevelStats.shotsFired >= 8 ? 0.7f : 0.0f;
            case OfferType::AdrenalineStitch:
                return completedLevelStats.nearMisses >= 2 ? 0.8f : 0.0f;
            default:
                return 0.0f;
        }
    }

    std::string buildRecommendationText(
        OfferType offerType,
        const LevelStats &completedLevelStats,
        const PlayerUpgradeState &upgradeState)
    {
        const int accuracyPercentage = UpgradeShop::getAccuracyPercentage(completedLevelStats);
        const bool noDamageClear =
            completedLevelStats.enemiesDefeated > 0 &&
            completedLevelStats.damageTaken == 0;

        switch (offerType)
        {
            case OfferType::SniperSigil:
                if (accuracyPercentage >= 70)
                {
                    return "Recommended: your last accuracy was " +
                           std::to_string(accuracyPercentage) + "%.";
                }
                if (noDamageClear)
                {
                    return "Recommended: no-damage clears can afford a sharper build.";
                }
                break;
            case OfferType::SovereignVolley:
                if (accuracyPercentage >= 80)
                {
                    return "Recommended: elite accuracy makes this your highest-ceiling damage card.";
                }
                if (upgradeState.echoShotCount > 0)
                {
                    return "Synergy: upgrades your existing echo build into a main path.";
                }
                break;
            case OfferType::PhoenixCore:
                if (completedLevelStats.damageTaken >= 20)
                {
                    return "Recommended: this stabilizes a rough level immediately.";
                }
                if (upgradeState.bonusMaxHealth >= 20)
                {
                    return "Synergy: stacks into a durable health-focused build.";
                }
                break;
            case OfferType::CelestialLattice:
                if (completedLevelStats.maxCombo >= 7)
                {
                    return "Recommended: strong combo runs benefit most from line-clearing power.";
                }
                if (upgradeState.projectilePierceCount > 0)
                {
                    return "Synergy: doubles down on your current piercing setup.";
                }
                break;
            case OfferType::BlinkReserve:
                if (completedLevelStats.dashKills >= 2 ||
                    completedLevelStats.nearMisses >= 2)
                {
                    return "Recommended: your last level leaned on dash movement.";
                }
                if (upgradeState.dashCooldownMultiplierOffset < 0.0f)
                {
                    return "Synergy: pairs well with your existing dash upgrades.";
                }
                break;
            case OfferType::ComboLoom:
                if (completedLevelStats.maxCombo >= 5)
                {
                    return "Recommended: you already kept a strong combo alive.";
                }
                if (playerOwnsProjectilePlanUpgrade(upgradeState))
                {
                    return "Synergy: extra projectiles make combo scaling pay off faster.";
                }
                break;
            case OfferType::ExecutionSigil:
                if (upgradeState.projectilePierceCount > 0)
                {
                    return "Synergy: piercing shots spread execution damage across lines.";
                }
                break;
            case OfferType::EchoWeave:
                if (upgradeState.bonusProjectileDamage >= 4)
                {
                    return "Synergy: flat damage makes each echo bolt hit harder.";
                }
                break;
            case OfferType::GlassFocus:
                if (noDamageClear)
                {
                    return "Recommended: your defense last level supports a glassier build.";
                }
                break;
            case OfferType::BloodPrice:
                if (upgradeState.bonusMaxHealth >= 20)
                {
                    return "Synergy: extra health makes this curse less punishing.";
                }
                break;
            case OfferType::AegisThread:
                if (completedLevelStats.damageTaken >= 12)
                {
                    return "Recommended: trims repeated hits from ranged pressure.";
                }
                if (upgradeState.bonusMaxHealth >= 20)
                {
                    return "Synergy: defense makes your larger health pool last longer.";
                }
                break;
            case OfferType::LifebloomTalisman:
                if (completedLevelStats.damageTaken >= 20)
                {
                    return "Recommended: future shopping now doubles as recovery.";
                }
                break;
            case OfferType::OverloadGlyph:
                if (completedLevelStats.shotsFired >= 8)
                {
                    return "Recommended: frequent casting turns this into steady burst damage.";
                }
                if (upgradeState.echoShotCount > 0 || upgradeState.projectilePierceCount > 0)
                {
                    return "Synergy: burst shots pay off harder with extra bolts or pierce.";
                }
                break;
            case OfferType::AdrenalineStitch:
                if (completedLevelStats.nearMisses >= 2)
                {
                    return "Recommended: rewards aggressive dash-heavy play.";
                }
                break;
            default:
                break;
        }

        return "";
    }

    ShopOffer buildShopOffer(
        const OfferDefinition &offerDefinition,
        int currentLevelNumber,
        const LevelStats &completedLevelStats,
        const PlayerUpgradeState &upgradeState)
    {
        const std::string recommendationText =
            buildRecommendationText(
                offerDefinition.offerType,
                completedLevelStats,
                upgradeState);

        return {
            offerDefinition.offerType,
            offerDefinition.rarity,
            offerDefinition.label,
            offerDefinition.description,
            recommendationText,
            offerDefinition.spriteKeyword,
            std::max(
                1,
                getLevelScaledCost(offerDefinition, currentLevelNumber) -
                    upgradeState.flatShopDiscount),
            !recommendationText.empty(),
        };
    }

    bool isDefinitionEligible(
        const OfferDefinition &offerDefinition,
        const LevelStats &completedLevelStats)
    {
        return isOfferUnlockedByPerformance(offerDefinition.offerType, completedLevelStats);
    }

    std::optional<std::size_t> pickWeightedDefinitionIndex(
        const std::vector<std::size_t> &candidateIndices,
        const LevelStats &completedLevelStats,
        const PlayerUpgradeState &upgradeState,
        std::mt19937 &randomNumberGenerator)
    {
        if (candidateIndices.empty())
        {
            return std::nullopt;
        }

        std::vector<float> weights;
        weights.reserve(candidateIndices.size());
        for (std::size_t candidateIndex : candidateIndices)
        {
            const OfferDefinition &offerDefinition = OfferDefinitions[candidateIndex];
            const float weight =
                getBaseOfferWeight(offerDefinition) +
                getPerformanceWeightBonus(offerDefinition.offerType, completedLevelStats) +
                getSynergyWeightBonus(offerDefinition.offerType, upgradeState);
            weights.push_back(std::max(0.05f, weight));
        }

        std::discrete_distribution<std::size_t> distribution(weights.begin(), weights.end());
        return candidateIndices[distribution(randomNumberGenerator)];
    }

    std::vector<std::size_t> filterDefinitionsByRarity(
        OfferRarity desiredRarity,
        const std::set<OfferType> &alreadyPickedOffers,
        const LevelStats &completedLevelStats)
    {
        std::vector<std::size_t> candidateIndices;
        for (std::size_t offerDefinitionIndex = 0;
             offerDefinitionIndex < OfferDefinitions.size();
             ++offerDefinitionIndex)
        {
            const OfferDefinition &offerDefinition = OfferDefinitions[offerDefinitionIndex];
            if (offerDefinition.rarity != desiredRarity ||
                alreadyPickedOffers.count(offerDefinition.offerType) > 0 ||
                !isDefinitionEligible(offerDefinition, completedLevelStats))
            {
                continue;
            }

            candidateIndices.push_back(offerDefinitionIndex);
        }

        return candidateIndices;
    }

    std::vector<std::size_t> filterDefinitionsAllowingMultipleRarities(
        const std::vector<OfferRarity> &desiredRarities,
        const std::set<OfferType> &alreadyPickedOffers,
        const LevelStats &completedLevelStats)
    {
        std::vector<std::size_t> candidateIndices;
        for (std::size_t offerDefinitionIndex = 0;
             offerDefinitionIndex < OfferDefinitions.size();
             ++offerDefinitionIndex)
        {
            const OfferDefinition &offerDefinition = OfferDefinitions[offerDefinitionIndex];
            if (alreadyPickedOffers.count(offerDefinition.offerType) > 0 ||
                !isDefinitionEligible(offerDefinition, completedLevelStats))
            {
                continue;
            }

            if (std::find(
                    desiredRarities.begin(),
                    desiredRarities.end(),
                    offerDefinition.rarity) == desiredRarities.end())
            {
                continue;
            }

            candidateIndices.push_back(offerDefinitionIndex);
        }

        return candidateIndices;
    }

    void registerPurchase(
        PlayerUpgradeState &upgradeState,
        OfferType offerType,
        int essencePaid)
    {
        upgradeState.purchaseHistory.push_back({offerType, essencePaid});
    }

    void applyOfferEffect(
        OfferType offerType,
        Player &player,
        PlayerUpgradeState &upgradeState,
        int direction)
    {
        const int signedDirection = direction >= 0 ? 1 : -1;
        switch (offerType)
        {
            case OfferType::VitalityStitch:
                upgradeState.bonusMaxHealth += 20 * signedDirection;
                UpgradeShop::applyPlayerUpgradeStats(player, upgradeState);
                if (signedDirection > 0)
                {
                    player.heal(20);
                }
                break;
            case OfferType::SharpenSpell:
                upgradeState.bonusProjectileDamage += 4 * signedDirection;
                break;
            case OfferType::FleetStep:
                upgradeState.dashCooldownMultiplierOffset += -0.12f * static_cast<float>(signedDirection);
                UpgradeShop::applyPlayerUpgradeStats(player, upgradeState);
                break;
            case OfferType::PiercingRune:
                upgradeState.projectilePierceCount += 1 * signedDirection;
                break;
            case OfferType::BlinkReserve:
                upgradeState.dashChargedShotBonus += 12 * signedDirection;
                break;
            case OfferType::ExecutionSigil:
                upgradeState.bonusDamageAgainstShielded += 6 * signedDirection;
                upgradeState.bonusDamageAgainstLowHealth += 6 * signedDirection;
                break;
            case OfferType::ComboLoom:
                upgradeState.comboThresholdForDamageBonus = 5;
                upgradeState.comboBonusProjectileDamage += 6 * signedDirection;
                if (upgradeState.comboBonusProjectileDamage <= 0)
                {
                    upgradeState.comboBonusProjectileDamage = 0;
                    upgradeState.comboThresholdForDamageBonus = 0;
                }
                break;
            case OfferType::EchoWeave:
                upgradeState.echoShotInterval = 4;
                upgradeState.echoShotCount += 1 * signedDirection;
                if (upgradeState.echoShotCount <= 0)
                {
                    upgradeState.echoShotCount = 0;
                    upgradeState.echoShotInterval = 0;
                }
                break;
            case OfferType::HeavyVeil:
                upgradeState.bonusMaxHealth += 40 * signedDirection;
                upgradeState.dashCooldownMultiplierOffset += 0.18f * static_cast<float>(signedDirection);
                UpgradeShop::applyPlayerUpgradeStats(player, upgradeState);
                break;
            case OfferType::GlassFocus:
                upgradeState.bonusProjectileDamage += 10 * signedDirection;
                upgradeState.bonusMaxHealth += -20 * signedDirection;
                UpgradeShop::applyPlayerUpgradeStats(player, upgradeState);
                break;
            case OfferType::SniperSigil:
                upgradeState.bonusProjectileDamage += 8 * signedDirection;
                break;
            case OfferType::SovereignVolley:
                upgradeState.bonusProjectileDamage += 12 * signedDirection;
                upgradeState.echoShotInterval = 3;
                upgradeState.echoShotCount += 1 * signedDirection;
                if (upgradeState.echoShotCount <= 0)
                {
                    upgradeState.echoShotCount = 0;
                    upgradeState.echoShotInterval = 0;
                }
                break;
            case OfferType::PhoenixCore:
                upgradeState.bonusMaxHealth += 30 * signedDirection;
                UpgradeShop::applyPlayerUpgradeStats(player, upgradeState);
                if (signedDirection > 0)
                {
                    player.heal(30);
                }
                break;
            case OfferType::CelestialLattice:
                upgradeState.projectilePierceCount += 1 * signedDirection;
                upgradeState.bonusDamageAgainstLowHealth += 8 * signedDirection;
                break;
            case OfferType::BloodPrice:
                upgradeState.bonusProjectileDamage += 14 * signedDirection;
                if (signedDirection > 0)
                {
                    player.takeDamage(18);
                }
                break;
            case OfferType::BorrowedTime:
                upgradeState.dashCooldownMultiplierOffset += -0.22f * static_cast<float>(signedDirection);
                upgradeState.bonusMaxHealth += -15 * signedDirection;
                UpgradeShop::applyPlayerUpgradeStats(player, upgradeState);
                break;
            case OfferType::AegisThread:
                upgradeState.bonusDefense += 2 * signedDirection;
                UpgradeShop::applyPlayerUpgradeStats(player, upgradeState);
                break;
            case OfferType::LifebloomTalisman:
                upgradeState.bonusHealOnPurchase += 8 * signedDirection;
                break;
            case OfferType::OverloadGlyph:
                upgradeState.burstShotInterval = 5;
                upgradeState.burstShotBonusDamage += 25 * signedDirection;
                if (upgradeState.burstShotBonusDamage <= 0)
                {
                    upgradeState.burstShotBonusDamage = 0;
                    upgradeState.burstShotInterval = 0;
                }
                break;
            case OfferType::AdrenalineStitch:
                upgradeState.bonusProjectileDamage += 5 * signedDirection;
                upgradeState.dashCooldownMultiplierOffset += -0.16f * static_cast<float>(signedDirection);
                upgradeState.bonusMaxHealth += -12 * signedDirection;
                UpgradeShop::applyPlayerUpgradeStats(player, upgradeState);
                break;
            case OfferType::RicochetRune:
                upgradeState.ricochetHitInterval = 4;
                upgradeState.ricochetDamage += 8 * signedDirection;
                if (upgradeState.ricochetDamage <= 0)
                {
                    upgradeState.ricochetDamage = 0;
                    upgradeState.ricochetHitInterval = 0;
                    upgradeState.projectileHitsSinceRicochet = 0;
                }
                break;
            case OfferType::VampireBolt:
                upgradeState.vampireHealOnKill += 2 * signedDirection;
                upgradeState.vampireHealOnKill =
                    std::max(0, upgradeState.vampireHealOnKill);
                break;
            case OfferType::ExecutionBloom:
                upgradeState.executionBloomDamage += 10 * signedDirection;
                upgradeState.executionBloomDamage =
                    std::max(0, upgradeState.executionBloomDamage);
                break;
            case OfferType::BossGrudge:
                upgradeState.bossBonusDamage += 12 * signedDirection;
                upgradeState.bossBonusDamage = std::max(0, upgradeState.bossBonusDamage);
                break;
            case OfferType::EmotionalSupportRock:
                upgradeState.bonusDefense += 1 * signedDirection;
                UpgradeShop::applyPlayerUpgradeStats(player, upgradeState);
                break;
            case OfferType::QuestionableSmoothie:
                upgradeState.bonusMaxHealth += -10 * signedDirection;
                UpgradeShop::applyPlayerUpgradeStats(player, upgradeState);
                if (signedDirection > 0)
                {
                    player.heal(35);
                }
                break;
            case OfferType::TaxEvasion:
                upgradeState.firstPurchaseDiscount += 3 * signedDirection;
                upgradeState.firstPurchaseDiscount =
                    std::max(0, upgradeState.firstPurchaseDiscount);
                break;
            case OfferType::SpiteButton:
                upgradeState.spiteButtonBonusDamage += 18 * signedDirection;
                upgradeState.spiteButtonBonusDamage =
                    std::max(0, upgradeState.spiteButtonBonusDamage);
                break;
            case OfferType::PanicEngine:
                upgradeState.panicDashCooldownMultiplierOffset += -0.25f * static_cast<float>(signedDirection);
                UpgradeShop::applyPlayerUpgradeStats(player, upgradeState);
                break;
            case OfferType::GremlinContract:
                upgradeState.flatShopDiscount += 2 * signedDirection;
                upgradeState.flatShopDiscount = std::max(0, upgradeState.flatShopDiscount);
                upgradeState.forceCursedOfferInShop = upgradeState.flatShopDiscount > 0;
                break;
            case OfferType::FragileCrown:
                upgradeState.fragileCrownBonusDamage += 24 * signedDirection;
                upgradeState.fragileCrownBonusDamage =
                    std::max(0, upgradeState.fragileCrownBonusDamage);
                break;
            case OfferType::SpitefulCore:
                upgradeState.spitefulCoreDamage += 6 * signedDirection;
                upgradeState.spitefulCoreDamage =
                    std::max(0, upgradeState.spitefulCoreDamage);
                break;
        }
    }

    void drawTextLine(
        sf::RenderTarget &target,
        const sf::Font &font,
        const std::string &text,
        unsigned int characterSize,
        sf::Vector2f position,
        sf::Color color)
    {
        sf::Text line(font);
        line.setString(text);
        line.setCharacterSize(characterSize);
        line.setFillColor(color);
        line.setPosition(position);
        target.draw(line);
    }

    void drawKeywordIcon(
        sf::RenderTarget &target,
        const sf::FloatRect &iconBounds,
        const std::string &spriteKeyword,
        sf::Color accentColor)
    {
        sf::RectangleShape iconFrame(iconBounds.size);
        iconFrame.setPosition(iconBounds.position);
        iconFrame.setFillColor(sf::Color(16, 18, 26, 235));
        iconFrame.setOutlineThickness(2.0f);
        iconFrame.setOutlineColor(accentColor);
        target.draw(iconFrame);

        const sf::Vector2f center = {
            iconBounds.position.x + iconBounds.size.x * 0.5f,
            iconBounds.position.y + iconBounds.size.y * 0.5f,
        };

        if (spriteKeyword == "heart")
        {
            sf::CircleShape leftLobe(11.0f);
            leftLobe.setFillColor(accentColor);
            leftLobe.setPosition({center.x - 18.0f, center.y - 16.0f});
            sf::CircleShape rightLobe(11.0f);
            rightLobe.setFillColor(accentColor);
            rightLobe.setPosition({center.x - 2.0f, center.y - 16.0f});
            sf::RectangleShape point({18.0f, 18.0f});
            point.setFillColor(accentColor);
            point.setOrigin({9.0f, 9.0f});
            point.setRotation(sf::degrees(45.0f));
            point.setPosition({center.x, center.y + 6.0f});
            target.draw(leftLobe);
            target.draw(rightLobe);
            target.draw(point);
            return;
        }

        if (spriteKeyword == "blade")
        {
            sf::RectangleShape blade({9.0f, 34.0f});
            blade.setFillColor(accentColor);
            blade.setOrigin({4.5f, 17.0f});
            blade.setPosition(center);
            sf::RectangleShape guard({24.0f, 6.0f});
            guard.setFillColor(sf::Color::White);
            guard.setOrigin({12.0f, 3.0f});
            guard.setPosition({center.x, center.y + 10.0f});
            target.draw(blade);
            target.draw(guard);
            return;
        }

        if (spriteKeyword == "dash" || spriteKeyword == "blink")
        {
            sf::RectangleShape shaft({24.0f, 8.0f});
            shaft.setFillColor(accentColor);
            shaft.setOrigin({12.0f, 4.0f});
            shaft.setPosition(center);
            sf::CircleShape arrowHead(10.0f, 3);
            arrowHead.setFillColor(accentColor);
            arrowHead.setOrigin({10.0f, 10.0f});
            arrowHead.setRotation(sf::degrees(90.0f));
            arrowHead.setPosition({center.x + 18.0f, center.y});
            target.draw(shaft);
            target.draw(arrowHead);
            return;
        }

        if (spriteKeyword == "pierce")
        {
            sf::RectangleShape spear({6.0f, 36.0f});
            spear.setFillColor(accentColor);
            spear.setOrigin({3.0f, 18.0f});
            spear.setPosition(center);
            sf::CircleShape tip(9.0f, 3);
            tip.setFillColor(sf::Color::White);
            tip.setOrigin({9.0f, 9.0f});
            tip.setRotation(sf::degrees(180.0f));
            tip.setPosition({center.x, center.y - 18.0f});
            target.draw(spear);
            target.draw(tip);
            return;
        }

        if (spriteKeyword == "execute" || spriteKeyword == "precision")
        {
            sf::CircleShape ring(17.0f);
            ring.setFillColor(sf::Color::Transparent);
            ring.setOutlineThickness(3.0f);
            ring.setOutlineColor(accentColor);
            ring.setOrigin({17.0f, 17.0f});
            ring.setPosition(center);
            sf::RectangleShape horizontal({28.0f, 3.0f});
            horizontal.setFillColor(sf::Color::White);
            horizontal.setOrigin({14.0f, 1.5f});
            horizontal.setPosition(center);
            sf::RectangleShape vertical({3.0f, 28.0f});
            vertical.setFillColor(sf::Color::White);
            vertical.setOrigin({1.5f, 14.0f});
            vertical.setPosition(center);
            target.draw(ring);
            target.draw(horizontal);
            target.draw(vertical);
            return;
        }

        if (spriteKeyword == "combo" || spriteKeyword == "echo")
        {
            for (int circleIndex = 0; circleIndex < 3; ++circleIndex)
            {
                sf::CircleShape orb(8.0f);
                orb.setFillColor(circleIndex == 1 ? sf::Color::White : accentColor);
                orb.setOrigin({8.0f, 8.0f});
                orb.setPosition({
                    center.x - 14.0f + static_cast<float>(circleIndex) * 14.0f,
                    center.y,
                });
                target.draw(orb);
            }
            return;
        }

        if (spriteKeyword == "armor")
        {
            sf::CircleShape shield(20.0f, 6);
            shield.setFillColor(accentColor);
            shield.setOrigin({20.0f, 20.0f});
            shield.setPosition(center);
            shield.setRotation(sf::degrees(30.0f));
            target.draw(shield);
            return;
        }

        if (spriteKeyword == "crown")
        {
            sf::RectangleShape base({34.0f, 10.0f});
            base.setFillColor(accentColor);
            base.setOrigin({17.0f, 5.0f});
            base.setPosition({center.x, center.y + 12.0f});
            target.draw(base);
            for (int pointIndex = 0; pointIndex < 3; ++pointIndex)
            {
                sf::CircleShape spike(10.0f, 3);
                spike.setFillColor(accentColor);
                spike.setOrigin({10.0f, 10.0f});
                spike.setRotation(sf::degrees(180.0f));
                spike.setPosition({
                    center.x - 14.0f + static_cast<float>(pointIndex) * 14.0f,
                    center.y - 2.0f,
                });
                target.draw(spike);
            }
            return;
        }

        if (spriteKeyword == "phoenix")
        {
            sf::CircleShape core(12.0f);
            core.setFillColor(sf::Color(255, 170, 90));
            core.setOrigin({12.0f, 12.0f});
            core.setPosition(center);
            sf::CircleShape leftWing(10.0f, 3);
            leftWing.setFillColor(accentColor);
            leftWing.setOrigin({10.0f, 10.0f});
            leftWing.setRotation(sf::degrees(240.0f));
            leftWing.setPosition({center.x - 14.0f, center.y});
            sf::CircleShape rightWing(10.0f, 3);
            rightWing.setFillColor(accentColor);
            rightWing.setOrigin({10.0f, 10.0f});
            rightWing.setRotation(sf::degrees(120.0f));
            rightWing.setPosition({center.x + 14.0f, center.y});
            target.draw(core);
            target.draw(leftWing);
            target.draw(rightWing);
            return;
        }

        if (spriteKeyword == "star")
        {
            sf::CircleShape star(18.0f, 5);
            star.setFillColor(accentColor);
            star.setOrigin({18.0f, 18.0f});
            star.setPosition(center);
            star.setRotation(sf::degrees(-90.0f));
            target.draw(star);
            return;
        }

        if (spriteKeyword == "glass" || spriteKeyword == "hourglass")
        {
            sf::RectangleShape diamond({26.0f, 26.0f});
            diamond.setFillColor(accentColor);
            diamond.setOrigin({13.0f, 13.0f});
            diamond.setRotation(sf::degrees(45.0f));
            diamond.setPosition(center);
            sf::RectangleShape crack({3.0f, 30.0f});
            crack.setFillColor(sf::Color(16, 18, 26));
            crack.setOrigin({1.5f, 15.0f});
            crack.setRotation(sf::degrees(20.0f));
            crack.setPosition(center);
            target.draw(diamond);
            target.draw(crack);
            return;
        }

        sf::CircleShape warning(18.0f, 3);
        warning.setFillColor(accentColor);
        warning.setOrigin({18.0f, 18.0f});
        warning.setRotation(sf::degrees(180.0f));
        warning.setPosition(center);
        target.draw(warning);
    }
}

namespace UpgradeShop
{
    void resetLevelStats(LevelStats &levelStats)
    {
        levelStats = {};
    }

    int getAccuracyPercentage(const LevelStats &levelStats)
    {
        if (levelStats.shotsFired <= 0)
        {
            return 0;
        }

        return static_cast<int>(
            std::round(
                100.0f * static_cast<float>(levelStats.shotsHit) /
                static_cast<float>(levelStats.shotsFired)));
    }

    std::vector<ShopOffer> buildShopOffersForLevel(
        int currentLevelNumber,
        const LevelStats &completedLevelStats,
        const PlayerUpgradeState &upgradeState,
        int,
        std::mt19937 &randomNumberGenerator)
    {
        std::vector<ShopOffer> shopOffers;
        std::set<OfferType> alreadyPickedOffers;

        // Slot 1 is intentionally readable: it always draws from common offers.
        if (const std::optional<std::size_t> commonIndex =
                pickWeightedDefinitionIndex(
                    filterDefinitionsByRarity(
                        OfferRarity::Common,
                        alreadyPickedOffers,
                        completedLevelStats),
                    completedLevelStats,
                    upgradeState,
                    randomNumberGenerator))
        {
            const OfferDefinition &offerDefinition = OfferDefinitions[*commonIndex];
            shopOffers.push_back(
                buildShopOffer(
                    offerDefinition,
                    currentLevelNumber,
                    completedLevelStats,
                    upgradeState));
            alreadyPickedOffers.insert(offerDefinition.offerType);
        }

        // Slot 2 is still usually safe, but it can now swing toward performance-relevant cards.
        if (const std::optional<std::size_t> flexibleIndex =
                pickWeightedDefinitionIndex(
                    filterDefinitionsAllowingMultipleRarities(
                        {OfferRarity::Common, OfferRarity::Rare, OfferRarity::Legendary},
                        alreadyPickedOffers,
                        completedLevelStats),
                    completedLevelStats,
                    upgradeState,
                    randomNumberGenerator))
        {
            const OfferDefinition &offerDefinition = OfferDefinitions[*flexibleIndex];
            shopOffers.push_back(
                buildShopOffer(
                    offerDefinition,
                    currentLevelNumber,
                    completedLevelStats,
                    upgradeState));
            alreadyPickedOffers.insert(offerDefinition.offerType);
        }

        // Slots 3 and 4 are the "surprise" lane: rare, legendary, or cursed if possible,
        // then fallback to anything still eligible. Extra visible choices make saving essence
        // matter because the player can now buy more than one upgrade from the same shop.
        for (int surpriseSlotIndex = 0; surpriseSlotIndex < 2; ++surpriseSlotIndex)
        {
            std::vector<std::size_t> surpriseCandidates =
                filterDefinitionsAllowingMultipleRarities(
                    {OfferRarity::Rare, OfferRarity::Legendary, OfferRarity::Cursed},
                    alreadyPickedOffers,
                    completedLevelStats);
            if (surpriseCandidates.empty())
            {
                surpriseCandidates = filterDefinitionsAllowingMultipleRarities(
                    {OfferRarity::Common, OfferRarity::Rare, OfferRarity::Legendary, OfferRarity::Cursed},
                    alreadyPickedOffers,
                    completedLevelStats);
            }

            if (const std::optional<std::size_t> surpriseIndex =
                    pickWeightedDefinitionIndex(
                        surpriseCandidates,
                        completedLevelStats,
                        upgradeState,
                        randomNumberGenerator))
            {
                const OfferDefinition &offerDefinition = OfferDefinitions[*surpriseIndex];
                shopOffers.push_back(
                    buildShopOffer(
                        offerDefinition,
                        currentLevelNumber,
                        completedLevelStats,
                        upgradeState));
                alreadyPickedOffers.insert(offerDefinition.offerType);
            }
        }

        if (upgradeState.forceCursedOfferInShop &&
            std::none_of(
                shopOffers.begin(),
                shopOffers.end(),
                [](const ShopOffer &shopOffer)
                {
                    return shopOffer.rarity == OfferRarity::Cursed;
                }))
        {
            std::set<OfferType> alreadyPickedCursedFixup;
            for (const ShopOffer &shopOffer : shopOffers)
            {
                alreadyPickedCursedFixup.insert(shopOffer.offerType);
            }

            if (const std::optional<std::size_t> cursedIndex =
                    pickWeightedDefinitionIndex(
                        filterDefinitionsByRarity(
                            OfferRarity::Cursed,
                            alreadyPickedCursedFixup,
                            completedLevelStats),
                        completedLevelStats,
                        upgradeState,
                        randomNumberGenerator))
            {
                shopOffers.back() =
                    buildShopOffer(
                        OfferDefinitions[*cursedIndex],
                        currentLevelNumber,
                        completedLevelStats,
                        upgradeState);
            }
        }

        return shopOffers;
    }

    void applyPlayerUpgradeStats(Player &player, const PlayerUpgradeState &upgradeState)
    {
        const int upgradedMaxHealth = 100 + upgradeState.bonusMaxHealth;
        player.setMaxHealth(upgradedMaxHealth);
        player.setDefense(upgradeState.bonusDefense);
        const float healthRatio =
            player.getMaxHealth() <= 0
                ? 1.0f
                : std::clamp(
                      static_cast<float>(player.getHealth()) /
                          static_cast<float>(player.getMaxHealth()),
                      0.0f,
                      1.0f);
        const float panicMultiplierOffset =
            healthRatio < 0.5f
                ? upgradeState.panicDashCooldownMultiplierOffset * (1.0f - healthRatio * 2.0f)
                : 0.0f;
        player.setDashCooldownDuration(
            Player::DefaultDashCooldown *
            std::clamp(
                1.0f + upgradeState.dashCooldownMultiplierOffset + panicMultiplierOffset,
                0.45f,
                1.65f));
    }

    bool tryPurchaseShopOffer(
        const ShopOffer &shopOffer,
        Player &player,
        int &playerEssence,
        PlayerUpgradeState &upgradeState,
        std::string &statusMessage)
    {
        if (playerEssence < shopOffer.essenceCost)
        {
            statusMessage = "Not enough essence for " + shopOffer.label + ".";
            return false;
        }

        playerEssence -= shopOffer.essenceCost;
        applyOfferEffect(shopOffer.offerType, player, upgradeState, 1);
        if (upgradeState.bonusHealOnPurchase > 0)
        {
            player.heal(upgradeState.bonusHealOnPurchase);
        }
        registerPurchase(upgradeState, shopOffer.offerType, shopOffer.essenceCost);
        statusMessage = "Purchased " + shopOffer.label + ".";
        return true;
    }

    bool tryTradeInLatestUpgrade(
        Player &player,
        int &playerEssence,
        PlayerUpgradeState &upgradeState,
        std::string &statusMessage)
    {
        if (upgradeState.purchaseHistory.empty())
        {
            statusMessage = "No previous upgrade is available to trade in.";
            return false;
        }

        const PurchasedUpgradeRecord tradedUpgrade = upgradeState.purchaseHistory.back();
        upgradeState.purchaseHistory.pop_back();
        applyOfferEffect(tradedUpgrade.offerType, player, upgradeState, -1);
        const int refundedEssence = std::max(1, tradedUpgrade.essencePaid / 2);
        playerEssence += refundedEssence;
        statusMessage =
            "Traded in your last upgrade for +" + std::to_string(refundedEssence) + " essence.";
        return true;
    }

    bool tryRerollShopOffers(
        int currentLevelNumber,
        const LevelStats &completedLevelStats,
        int &playerEssence,
        const PlayerUpgradeState &upgradeState,
        ShopState &shopState,
        std::mt19937 &randomNumberGenerator,
        std::string &statusMessage)
    {
        const int rerollCost = 3 + shopState.rerollCount * 2;
        if (playerEssence < rerollCost)
        {
            statusMessage = "Reroll costs " + std::to_string(rerollCost) + " essence.";
            return false;
        }

        playerEssence -= rerollCost;
        ++shopState.rerollCount;
        shopState.currentOffers =
            buildShopOffersForLevel(
                currentLevelNumber,
                completedLevelStats,
                upgradeState,
                playerEssence,
                randomNumberGenerator);
        statusMessage =
            "Rerolled the shop for " + std::to_string(rerollCost) + " essence.";
        return true;
    }

    void applySkipRewards(
        Player &player,
        int &playerEssence,
        std::string &statusMessage)
    {
        // Skipping still does something so the decision is not just "waste a menu."
        const int interestReward = playerEssence >= 10 ? 2 : 1;
        playerEssence += interestReward;
        player.heal(10);
        statusMessage =
            "Skipped the shop: +" + std::to_string(interestReward) + " essence and healed 10.";
    }

    void notifyPlayerDashPerformed(PlayerUpgradeState &upgradeState)
    {
        if (upgradeState.dashChargedShotBonus > 0)
        {
            upgradeState.pendingNextShotBonusDamage += upgradeState.dashChargedShotBonus;
        }
    }

    FiredShotPlan consumeNextShotPlan(
        const LevelStats &currentLevelStats,
        PlayerUpgradeState &upgradeState)
    {
        ++upgradeState.totalShotsFiredThisRun;

        FiredShotPlan shotPlan;
        shotPlan.projectileDamage = 10 + upgradeState.bonusProjectileDamage;
        if (upgradeState.comboThresholdForDamageBonus > 0 &&
            currentLevelStats.currentCombo >= upgradeState.comboThresholdForDamageBonus)
        {
            shotPlan.projectileDamage += upgradeState.comboBonusProjectileDamage;
        }
        if (upgradeState.burstShotInterval > 0 &&
            upgradeState.burstShotBonusDamage > 0 &&
            upgradeState.totalShotsFiredThisRun % upgradeState.burstShotInterval == 0)
        {
            shotPlan.projectileDamage += upgradeState.burstShotBonusDamage;
        }
        if (!currentLevelStats.tookDamage && upgradeState.fragileCrownBonusDamage > 0)
        {
            shotPlan.projectileDamage += upgradeState.fragileCrownBonusDamage;
        }

        shotPlan.projectileDamage += upgradeState.pendingNextShotBonusDamage;
        shotPlan.projectilePierceCount = upgradeState.projectilePierceCount;

        if (upgradeState.echoShotInterval > 0 &&
            upgradeState.echoShotCount > 0 &&
            upgradeState.totalShotsFiredThisRun % upgradeState.echoShotInterval == 0)
        {
            shotPlan.echoProjectileCount = upgradeState.echoShotCount;
        }

        upgradeState.pendingNextShotBonusDamage = 0;
        return shotPlan;
    }

    int calculateProjectileDamageBonusOnHit(
        const Enemy &enemy,
        const PlayerUpgradeState &upgradeState)
    {
        int bonusDamage = 0;
        if (enemy.hasShield())
        {
            bonusDamage += upgradeState.bonusDamageAgainstShielded;
        }

        if (enemy.getEnemyType() == Enemy::EnemyType::MiniBoss ||
            enemy.getEnemyType() == Enemy::EnemyType::FinalBoss)
        {
            bonusDamage += upgradeState.bossBonusDamage;
        }

        const int baseEnemyHealth = Enemy::getBaseHealthForEnemyType(enemy.getEnemyType());
        if (enemy.getHealth() <= std::max(1, baseEnemyHealth / 2))
        {
            bonusDamage += upgradeState.bonusDamageAgainstLowHealth;
        }

        return bonusDamage;
    }

    std::string buildShopMessage(const ShopState &shopState, int playerEssence)
    {
        std::string shopMessage =
            "Upgrade Shop | Essence " + std::to_string(playerEssence) + "\n";
        for (std::size_t offerIndex = 0; offerIndex < shopState.currentOffers.size(); ++offerIndex)
        {
            const ShopOffer &shopOffer = shopState.currentOffers[offerIndex];
            const int displayedCost =
                !shopState.purchaseMade && shopState.firstPurchaseDiscount > 0
                    ? std::max(1, shopOffer.essenceCost - shopState.firstPurchaseDiscount)
                    : shopOffer.essenceCost;
            shopMessage +=
                std::to_string(static_cast<int>(offerIndex + 1)) + ". " +
                shopOffer.label + " [" + std::to_string(displayedCost) + "]\n" +
                shopOffer.description + "\n";
        }
        shopMessage += "R reroll | T trade in | Enter continue";
        return shopMessage;
    }

    std::string getOfferRarityLabel(OfferRarity offerRarity)
    {
        switch (offerRarity)
        {
            case OfferRarity::Common:
                return "Common";
            case OfferRarity::Rare:
                return "Rare";
            case OfferRarity::Legendary:
                return "Legendary";
            case OfferRarity::Cursed:
                return "Cursed";
        }

        return "Common";
    }

    sf::Color getOfferAccentColor(OfferRarity offerRarity)
    {
        switch (offerRarity)
        {
            case OfferRarity::Common:
                return sf::Color(125, 205, 255);
            case OfferRarity::Rare:
                return sf::Color(255, 210, 110);
            case OfferRarity::Legendary:
                return sf::Color(255, 245, 150);
            case OfferRarity::Cursed:
                return sf::Color(255, 110, 130);
        }

        return sf::Color::White;
    }

    sf::FloatRect getPanelBounds()
    {
        return {{OverlayPanelX, OverlayPanelY}, {OverlayPanelWidth, OverlayPanelHeight}};
    }

    sf::FloatRect getContinueButtonBounds()
    {
        return {
            {OverlayPanelX + OverlayPanelWidth - 182.0f, OverlayPanelY + OverlayPanelHeight - 56.0f},
            {150.0f, 38.0f},
        };
    }

    sf::FloatRect getCardBounds(std::size_t offerIndex)
    {
        return {
            {OverlayPanelX + 28.0f, OverlayPanelY + 108.0f + static_cast<float>(offerIndex) * 88.0f},
            {OverlayPanelWidth - 56.0f, 78.0f},
        };
    }

    sf::FloatRect getRerollButtonBounds()
    {
        return {
            {OverlayPanelX + 28.0f, OverlayPanelY + OverlayPanelHeight - 56.0f},
            {142.0f, 38.0f},
        };
    }

    sf::FloatRect getTradeInButtonBounds()
    {
        return {
            {OverlayPanelX + 184.0f, OverlayPanelY + OverlayPanelHeight - 56.0f},
            {172.0f, 38.0f},
        };
    }

    void drawShopOverlay(
        sf::RenderWindow &window,
        const sf::Font &font,
        const ShopState &shopState,
        int playerEssence)
    {
        const sf::FloatRect panelBounds = getPanelBounds();

        drawTextLine(
            window,
            font,
            "Upgrade Shop  |  Essence " + std::to_string(playerEssence),
            30,
            {panelBounds.position.x + 28.0f, panelBounds.position.y + 22.0f},
            sf::Color::White);

        const int rerollCost = 3 + shopState.rerollCount * 2;
        const std::string headerHint =
            shopState.purchaseMade
                ? "Bought at least one upgrade. Buy more if you can afford them, or continue."
                : "Press 1-4 to buy, R to reroll, T to trade in, or Enter to skip.";
        drawTextLine(
            window,
            font,
            headerHint,
            16,
            {panelBounds.position.x + 30.0f, panelBounds.position.y + 68.0f},
            sf::Color(205, 216, 230));

        if (!shopState.statusText.empty())
        {
            drawTextLine(
                window,
                font,
                shopState.statusText,
                15,
                {panelBounds.position.x + 30.0f, panelBounds.position.y + 88.0f},
                sf::Color(170, 225, 190));
        }

        for (std::size_t offerIndex = 0; offerIndex < shopState.currentOffers.size(); ++offerIndex)
        {
            const ShopOffer &shopOffer = shopState.currentOffers[offerIndex];
            const sf::FloatRect cardBounds = getCardBounds(offerIndex);
            const sf::Color accentColor = getOfferAccentColor(shopOffer.rarity);
            const int displayedCost =
                !shopState.purchaseMade && shopState.firstPurchaseDiscount > 0
                    ? std::max(1, shopOffer.essenceCost - shopState.firstPurchaseDiscount)
                    : shopOffer.essenceCost;
            const bool canAfford = playerEssence >= displayedCost;

            sf::RectangleShape card(cardBounds.size);
            card.setPosition(cardBounds.position);
            card.setFillColor(canAfford ? sf::Color(26, 32, 48, 240) : sf::Color(24, 24, 28, 220));
            card.setOutlineThickness(2.0f);
            card.setOutlineColor(canAfford ? accentColor : sf::Color(92, 92, 100));
            window.draw(card);

            const sf::FloatRect iconBounds = {
                {cardBounds.position.x + 14.0f, cardBounds.position.y + 12.0f},
                {54.0f, 54.0f},
            };
            drawKeywordIcon(window, iconBounds, shopOffer.spriteKeyword, accentColor);

            drawTextLine(
                window,
                font,
                std::to_string(static_cast<int>(offerIndex + 1)) + ". " + shopOffer.label +
                    "  [" + std::to_string(displayedCost) + "]",
                22,
                {cardBounds.position.x + 82.0f, cardBounds.position.y + 8.0f},
                canAfford ? sf::Color::White : sf::Color(180, 180, 180));

            drawTextLine(
                window,
                font,
                getOfferRarityLabel(shopOffer.rarity) + "  |  icon key: " + shopOffer.spriteKeyword,
                14,
                {cardBounds.position.x + 82.0f, cardBounds.position.y + 34.0f},
                accentColor);

            drawTextLine(
                window,
                font,
                shopOffer.description,
                16,
                {cardBounds.position.x + 82.0f, cardBounds.position.y + 52.0f},
                canAfford ? sf::Color(215, 225, 238) : sf::Color(145, 145, 150));

            if (shopOffer.isRecommended)
            {
                drawTextLine(
                    window,
                    font,
                    shopOffer.recommendationText,
                    14,
                    {cardBounds.position.x + 82.0f, cardBounds.position.y + 64.0f},
                    sf::Color(180, 235, 185));
            }
        }

        const auto drawActionButton =
            [&](const sf::FloatRect &bounds,
                const std::string &title,
                const std::string &subtitle,
                bool enabled)
        {
            sf::RectangleShape button(bounds.size);
            button.setPosition(bounds.position);
            button.setFillColor(enabled ? sf::Color(42, 50, 70, 225) : sf::Color(30, 32, 36, 215));
            button.setOutlineThickness(2.0f);
            button.setOutlineColor(enabled ? sf::Color(145, 210, 255) : sf::Color(90, 90, 90));
            window.draw(button);

            drawTextLine(
                window,
                font,
                title,
                18,
                {bounds.position.x + 12.0f, bounds.position.y + 6.0f},
                enabled ? sf::Color::White : sf::Color(170, 170, 170));
            drawTextLine(
                window,
                font,
                subtitle,
                13,
                {bounds.position.x + 12.0f, bounds.position.y + 24.0f},
                enabled ? sf::Color(205, 220, 235) : sf::Color(130, 130, 130));
        };

        drawActionButton(
            getRerollButtonBounds(),
            "Reroll",
            "Cost " + std::to_string(rerollCost),
            !shopState.purchaseMade);
        drawActionButton(
            getTradeInButtonBounds(),
            "Trade In",
            "Refund half of last buy",
            !shopState.purchaseMade && !shopState.tradeInUsed);
        drawActionButton(
            getContinueButtonBounds(),
            "Continue",
            shopState.purchaseMade ? "Begin the next level." : "Skip for interest + a heal.",
            true);
    }
}
