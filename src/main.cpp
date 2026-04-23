#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "UpgradeShop.hpp"
#include "UI/DashCooldownIndicator.hpp"
#include "UI/EnemyDeathEffect.hpp"
#include "UI/ScreenShakeController.hpp"
#include "entities/Enemy.hpp"
#include "entities/Player.hpp"
#include "entities/Projectile.hpp"

namespace
{
    constexpr float EnemyFormationHorizontalSpeed = 50.0f;
    constexpr float EnemyFormationDropDistance = 28.0f;
    constexpr float MovingFormationStartY = 120.0f;
    constexpr float StationaryBossEncounterStartY = 80.0f;
    constexpr float VerticalSpacingBetweenRows = 70.0f;
    constexpr float ShortHitPauseDurationSeconds = 0.03f;
    constexpr float LargeHitPauseDurationSeconds = 0.05f;
    constexpr float DashNearMissPadding = 34.0f;
    constexpr float DashNearMissCooldownRewardSeconds = 0.18f;
    constexpr std::array<const char *, 2> AssailSong1MusicPaths = {
        "resources/audio/AssailSong1 - 4-16-26, 3.23 PM.ogg",
        "../resources/audio/AssailSong1 - 4-16-26, 3.23 PM.ogg",
    };
    constexpr const char *NoBackgroundMusicTrackId = "";
    constexpr const char *AssailSong1TrackId = "ASSAILSONG1";

    struct FloatingHudRewardText
    {
        explicit FloatingHudRewardText(const sf::Font &font)
            : text(font)
        {
        }

        // The text itself is the renderable payload. The rest of the fields define
        // how long it floats and what direction it drifts while fading out.
        sf::Text text;
        sf::Vector2f velocity;
        float lifetimeSeconds = 0.0f;
        float totalLifetimeSeconds = 0.85f;
    };

    struct FrameCombatFeedback
    {
        // These flags summarize what happened during the current gameplay step so the
        // main loop can apply hit pause, screen shake, score gain, and popup text once.
        bool playerTookDamageThisFrame = false;
        bool bossAttackTriggeredThisFrame = false;
        bool bossTookDamageThisFrame = false;
        bool dashHitEnemyThisFrame = false;
        int scoreEarnedThisFrame = 0;
        int essenceEarnedThisFrame = 0;
        int playerShotsHitThisFrame = 0;
        int dashKillsThisFrame = 0;
        int nearMissesThisFrame = 0;
        int playerDamageTakenThisFrame = 0;
        int enemiesDefeatedThisFrame = 0;
        std::vector<EnemyDeathEffect> enemyDeathEffectsToSpawn;
        std::vector<FloatingHudRewardText> floatingRewardTextsToSpawn;
    };

    enum class LevelEncounterStyle
    {
        MovingFormation,
        StationaryBossEncounter,
    };

    enum class LevelProgressState
    {
        Playing,
        ShowingLevelStats,
        ShowingShopIntro,
        ShowingShop,
        ShowingLevelIntro,
        Victory,
        PlayerDefeated,
    };

    enum class AppFlowState
    {
        TitleScreen,
        CodeEntry,
        InRun,
    };

    enum class TitleMenuSelection
    {
        Start,
        EnterCode,
        Exit,
    };

    struct EnemyRowSpawnConfiguration
    {
        struct EnemySpawnConfiguration
        {
            Enemy::EnemyType enemyType = Enemy::EnemyType::BasicGrunt;
            Enemy::EnemyBehaviorProfile behaviorProfile = Enemy::EnemyBehaviorProfile::Basic;
        };

        float horizontalSpacingBetweenEnemies;
        std::vector<EnemySpawnConfiguration> enemiesInThisRow;
    };

    struct LevelConfiguration
    {
        int levelNumber;
        std::string levelDisplayName;
        std::string backgroundMusicTrackId;
        float enemyProjectileCooldownMultiplier;
        float levelScaleMultiplier;
        LevelEncounterStyle levelEncounterStyle;
        std::vector<EnemyRowSpawnConfiguration> enemyRowsToSpawn;
        bool levelContainsMiniBoss;
        bool levelContainsFinalBoss;
    };

    struct RunProgress
    {
        UpgradeShop::PlayerUpgradeState upgrades;
        UpgradeShop::LevelStats currentLevelStats;
        UpgradeShop::LevelStats completedLevelStats;
        UpgradeShop::ShopState shopState;
    };

    using LevelStats = UpgradeShop::LevelStats;

    struct OverlayButton
    {
        sf::FloatRect bounds;
        std::string title;
        std::string subtitle;
        bool enabled = true;
    };

    struct RunCodeState
    {
        bool devModeEnabled = false;
        bool emotionalDamageEnabled = false;
        int attacksLaunchedThisRun = 0;
        std::string titleStatusMessage = "No codes active.";
        std::string codeEntryBuffer;
        std::string codeEntryStatusMessage;
    };

    std::string buildLevelBannerText(const LevelConfiguration &levelConfiguration);
    void centerTextHorizontally(sf::Text &text, float windowWidth, float yPosition);

    int getScoreValueForEnemyType(Enemy::EnemyType enemyType)
    {
        switch (enemyType)
        {
        case Enemy::EnemyType::BasicGrunt:
            return 100;
        case Enemy::EnemyType::MiniBoss:
            return 750;
        case Enemy::EnemyType::FinalBoss:
            return 2000;
        }

        return 0;
    }

    int getEssenceValueForEnemyType(Enemy::EnemyType enemyType)
    {
        switch (enemyType)
        {
        case Enemy::EnemyType::BasicGrunt:
            return 1;
        case Enemy::EnemyType::MiniBoss:
            return 8;
        case Enemy::EnemyType::FinalBoss:
            return 20;
        }

        return 0;
    }

    sf::Color getRewardTextColorForEnemyType(Enemy::EnemyType enemyType)
    {
        switch (enemyType)
        {
        case Enemy::EnemyType::BasicGrunt:
            return sf::Color(120, 235, 170);
        case Enemy::EnemyType::MiniBoss:
            return sf::Color(255, 205, 110);
        case Enemy::EnemyType::FinalBoss:
            return sf::Color(255, 150, 150);
        }

        return sf::Color::White;
    }

    sf::Color getRewardTextColorForEnemyBehavior(Enemy::EnemyBehaviorProfile behaviorProfile)
    {
        switch (behaviorProfile)
        {
        case Enemy::EnemyBehaviorProfile::Basic:
            return sf::Color(120, 235, 170);
        case Enemy::EnemyBehaviorProfile::Shielded:
            return sf::Color(150, 210, 255);
        case Enemy::EnemyBehaviorProfile::Shooter:
            return sf::Color(255, 165, 150);
        case Enemy::EnemyBehaviorProfile::Agile:
            return sf::Color(255, 220, 130);
        }

        return sf::Color::White;
    }

    std::string getEnemyBehaviorLabel(Enemy::EnemyBehaviorProfile behaviorProfile)
    {
        switch (behaviorProfile)
        {
        case Enemy::EnemyBehaviorProfile::Basic:
            return "Basic";
        case Enemy::EnemyBehaviorProfile::Shielded:
            return "Shield Break";
        case Enemy::EnemyBehaviorProfile::Shooter:
            return "Shooter";
        case Enemy::EnemyBehaviorProfile::Agile:
            return "Agile";
        }

        return "Enemy";
    }

    EnemyRowSpawnConfiguration::EnemySpawnConfiguration createEnemySpawn(
        Enemy::EnemyBehaviorProfile behaviorProfile,
        Enemy::EnemyType enemyType = Enemy::EnemyType::BasicGrunt)
    {
        return {enemyType, behaviorProfile};
    }

    EnemyRowSpawnConfiguration createEnemyRow(
        std::initializer_list<EnemyRowSpawnConfiguration::EnemySpawnConfiguration> enemiesInThisRow,
        float horizontalSpacingBetweenEnemies = 32.0f)
    {
        return {horizontalSpacingBetweenEnemies, enemiesInThisRow};
    }

    EnemyRowSpawnConfiguration createRepeatedEnemyRow(
        int numberOfEnemies,
        Enemy::EnemyBehaviorProfile behaviorProfile,
        Enemy::EnemyType enemyType = Enemy::EnemyType::BasicGrunt,
        float horizontalSpacingBetweenEnemies = 32.0f)
    {
        EnemyRowSpawnConfiguration rowConfiguration;
        rowConfiguration.horizontalSpacingBetweenEnemies = horizontalSpacingBetweenEnemies;
        rowConfiguration.enemiesInThisRow.reserve(std::max(0, numberOfEnemies));
        for (int enemyIndex = 0; enemyIndex < numberOfEnemies; ++enemyIndex)
        {
            rowConfiguration.enemiesInThisRow.push_back({enemyType, behaviorProfile});
        }

        return rowConfiguration;
    }

    float getDifficultyScale(
        int currentLevelIndex,
        int playerScore,
        const RunProgress &runProgress)
    {
        // Difficulty ramps from three sources: deeper levels, higher score, and strong
        // execution on the last stage. The values stay intentionally mild so the game
        // feels more alive without invalidating hand-authored encounters.
        const float levelContribution = static_cast<float>(currentLevelIndex) * 0.04f;
        const float scoreContribution =
            std::min(0.20f, static_cast<float>(playerScore) / 12000.0f);
        const float performanceContribution =
            runProgress.completedLevelStats.damageTaken == 0 &&
                    runProgress.completedLevelStats.enemiesDefeated > 0
                ? 0.08f
                : 0.0f;
        return 1.0f + levelContribution + scoreContribution + performanceContribution;
    }

    float getEnemyHealthRatio(const Enemy &enemy)
    {
        if (enemy.getMaxHealth() <= 0)
        {
            return 0.0f;
        }

        return std::clamp(
            static_cast<float>(enemy.getHealth()) / static_cast<float>(enemy.getMaxHealth()),
            0.0f,
            1.0f);
    }

    std::string normalizeCodeText(const std::string &rawCodeText)
    {
        std::string normalizedCodeText;
        normalizedCodeText.reserve(rawCodeText.size());
        for (unsigned char character : rawCodeText)
        {
            if (std::isspace(character))
            {
                continue;
            }

            normalizedCodeText.push_back(static_cast<char>(std::toupper(character)));
        }

        return normalizedCodeText;
    }

    std::vector<std::string> buildActiveCodeLabels(const RunCodeState &runCodeState)
    {
        std::vector<std::string> activeCodeLabels;
        if (runCodeState.devModeEnabled)
        {
            activeCodeLabels.push_back("DevMode");
        }
        if (runCodeState.emotionalDamageEnabled)
        {
            activeCodeLabels.push_back("EmotionalDamage");
        }
        return activeCodeLabels;
    }

    bool applyRunCode(RunCodeState &runCodeState, const std::string &rawCodeText)
    {
        const std::string normalizedCodeText = normalizeCodeText(rawCodeText);
        if (normalizedCodeText.empty())
        {
            runCodeState.codeEntryStatusMessage = "Enter a code first.";
            return false;
        }

        if (normalizedCodeText == "DEVMODE")
        {
            runCodeState.devModeEnabled = true;
            runCodeState.titleStatusMessage = "DevMode active: press N during a run to skip ahead.";
            runCodeState.codeEntryStatusMessage = "Accepted: DevMode";
            return true;
        }

        if (normalizedCodeText == "EMOTIONALDAMAGE")
        {
            runCodeState.emotionalDamageEnabled = true;
            runCodeState.titleStatusMessage =
                "EmotionalDamage active: every 3rd attack launches a full spell wall.";
            runCodeState.codeEntryStatusMessage = "Accepted: EmotionalDamage";
            return true;
        }

        runCodeState.codeEntryStatusMessage = "Unknown code.";
        return false;
    }

    Projectile createProjectileFromAngle(
        sf::Vector2f position,
        float speed,
        float angleDegrees,
        float radius = 6.0f,
        sf::Color color = sf::Color(255, 120, 120),
        int damage = Projectile::DefaultEnemyProjectileDamage,
        Projectile::VisualVariant visualVariant = Projectile::VisualVariant::EnemyShooter)
    {
        const float radians = angleDegrees * 3.14159265f / 180.0f;
        const sf::Vector2f direction = {std::sin(radians), std::cos(radians)};
        return Projectile(
            position,
            direction * speed,
            damage,
            radius,
            color,
            0.0f,
            0,
            visualVariant);
    }

    void addBossLaneWall(
        std::vector<Projectile> &spawnedProjectiles,
        const Enemy &enemy,
        float speed,
        int laneCount,
        float horizontalSpread,
        Projectile::VisualVariant visualVariant)
    {
        const sf::FloatRect bossBounds = enemy.getBounds();
        const float laneStep =
            laneCount <= 1 ? 0.0f : horizontalSpread / static_cast<float>(laneCount - 1);
        const float startX =
            bossBounds.position.x + bossBounds.size.x * 0.5f - horizontalSpread * 0.5f;
        const float spawnY = bossBounds.position.y + bossBounds.size.y;

        for (int laneIndex = 0; laneIndex < laneCount; ++laneIndex)
        {
            spawnedProjectiles.emplace_back(
                sf::Vector2f{startX + laneStep * static_cast<float>(laneIndex), spawnY},
                sf::Vector2f{0.0f, speed},
                Projectile::DefaultEnemyProjectileDamage,
                6.0f,
                sf::Color(255, 145, 120),
                0.0f,
                0,
                visualVariant);
        }
    }

    std::vector<Projectile> createEnemyAttackProjectiles(
        Enemy &enemy,
        const LevelConfiguration &levelConfiguration,
        sf::Vector2f playerSnapshotPosition,
        float difficultyScale)
    {
        std::vector<Projectile> spawnedProjectiles;
        const float projectileSpeed = 360.0f * std::max(0.2f, difficultyScale);

        if (enemy.getEnemyType() == Enemy::EnemyType::FinalBoss)
        {
            constexpr Projectile::VisualVariant finalBossProjectileVariant =
                Projectile::VisualVariant::FinalBoss;
            const int volleyPatternIndex = enemy.consumeVolleyPatternIndex();
            const float healthRatio = getEnemyHealthRatio(enemy);
            const bool enraged = healthRatio <= 0.35f;
            const bool pressured = healthRatio <= 0.65f;

            switch (volleyPatternIndex % 4)
            {
            case 0:
            {
                const std::vector<float> fanAngles =
                    enraged ? std::vector<float>{-46.0f, -30.0f, -16.0f, 0.0f, 16.0f, 30.0f, 46.0f}
                            : std::vector<float>{-34.0f, -18.0f, 0.0f, 18.0f, 34.0f};
                for (float fanAngle : fanAngles)
                {
                    spawnedProjectiles.push_back(createProjectileFromAngle(
                        enemy.getProjectileSpawnPosition(),
                        projectileSpeed * (enraged ? 1.28f : 1.15f),
                        fanAngle,
                        6.0f,
                        sf::Color(255, 145, 120),
                        Projectile::DefaultEnemyProjectileDamage,
                        finalBossProjectileVariant));
                }
                break;
            }
            case 1:
            {
                addBossLaneWall(
                    spawnedProjectiles,
                    enemy,
                    projectileSpeed * (enraged ? 1.15f : 1.0f),
                    enraged ? 10 : 8,
                    enemy.getBounds().size.x + (pressured ? 180.0f : 120.0f),
                    finalBossProjectileVariant);
                break;
            }
            case 2:
            {
                const std::vector<float> offsetAngles =
                    pressured ? std::vector<float>{-28.0f, -12.0f, 0.0f, 12.0f, 28.0f}
                              : std::vector<float>{-18.0f, 0.0f, 18.0f};
                for (float offsetAngle : offsetAngles)
                {
                    spawnedProjectiles.push_back(Projectile::createEnemyProjectileTowardTarget(
                        enemy.getProjectileSpawnPosition(),
                        playerSnapshotPosition,
                        difficultyScale * (enraged ? 1.32f : 1.15f),
                        offsetAngle,
                        finalBossProjectileVariant));
                }
                break;
            }
            default:
            {
                const sf::FloatRect bossBounds = enemy.getBounds();
                const float spawnY = bossBounds.position.y + bossBounds.size.y * 0.8f;
                const std::array<float, 4> xOffsets = {-110.0f, -36.0f, 36.0f, 110.0f};
                const float sweepBias = volleyPatternIndex % 8 < 4 ? -16.0f : 16.0f;
                for (float xOffset : xOffsets)
                {
                    spawnedProjectiles.push_back(createProjectileFromAngle(
                        {
                            bossBounds.position.x + bossBounds.size.x * 0.5f + xOffset,
                            spawnY,
                        },
                        projectileSpeed * (pressured ? 1.15f : 1.0f),
                        sweepBias + xOffset * 0.08f,
                        6.0f,
                        sf::Color(255, 145, 120),
                        Projectile::DefaultEnemyProjectileDamage,
                        finalBossProjectileVariant));
                }
                if (enraged)
                {
                    spawnedProjectiles.push_back(Projectile::createEnemyProjectileTowardTarget(
                        enemy.getProjectileSpawnPosition(),
                        playerSnapshotPosition,
                        difficultyScale * 1.4f,
                        0.0f,
                        finalBossProjectileVariant));
                }
                break;
            }
            }

            return spawnedProjectiles;
        }

        if (enemy.getEnemyType() == Enemy::EnemyType::MiniBoss)
        {
            constexpr Projectile::VisualVariant miniBossProjectileVariant =
                Projectile::VisualVariant::MiniBoss;
            const int volleyPatternIndex = enemy.consumeVolleyPatternIndex();
            const float healthRatio = getEnemyHealthRatio(enemy);
            const bool pressured = healthRatio <= 0.6f;

            switch (volleyPatternIndex % 3)
            {
            case 0:
            {
                const std::vector<float> angles =
                    pressured ? std::vector<float>{-30.0f, -16.0f, 0.0f, 16.0f, 30.0f}
                              : std::vector<float>{-20.0f, 0.0f, 20.0f};
                for (float angle : angles)
                {
                    spawnedProjectiles.push_back(Projectile::createEnemyProjectileTowardTarget(
                        enemy.getProjectileSpawnPosition(),
                        playerSnapshotPosition,
                        difficultyScale * 1.08f,
                        angle,
                        miniBossProjectileVariant));
                }
                break;
            }
            case 1:
            {
                addBossLaneWall(
                    spawnedProjectiles,
                    enemy,
                    projectileSpeed * 0.92f,
                    pressured ? 6 : 5,
                    enemy.getBounds().size.x + 90.0f,
                    miniBossProjectileVariant);
                break;
            }
            default:
            {
                const sf::FloatRect bossBounds = enemy.getBounds();
                const std::array<float, 4> spawnOffsets = {-72.0f, -28.0f, 28.0f, 72.0f};
                for (float spawnOffset : spawnOffsets)
                {
                    spawnedProjectiles.push_back(Projectile::createEnemyProjectileTowardTarget(
                        {
                            bossBounds.position.x + bossBounds.size.x * 0.5f + spawnOffset,
                            bossBounds.position.y + bossBounds.size.y,
                        },
                        playerSnapshotPosition,
                        difficultyScale * (pressured ? 1.16f : 1.0f),
                        spawnOffset * 0.05f,
                        miniBossProjectileVariant));
                }
                break;
            }
            }

            return spawnedProjectiles;
        }

        const std::vector<float> projectilePatternAngles =
            enemy.getBehaviorProfile() == Enemy::EnemyBehaviorProfile::Shooter &&
                    levelConfiguration.levelNumber >= 8
                ? std::vector<float>{-18.0f, 0.0f, 18.0f}
            : enemy.getBehaviorProfile() == Enemy::EnemyBehaviorProfile::Shooter
                ? std::vector<float>{-10.0f, 0.0f, 10.0f}
                : std::vector<float>{0.0f};

        for (const float projectileAngle : projectilePatternAngles)
        {
            spawnedProjectiles.push_back(Projectile::createEnemyProjectileTowardTarget(
                enemy.getProjectileSpawnPosition(),
                playerSnapshotPosition,
                difficultyScale,
                projectileAngle));
        }
        return spawnedProjectiles;
    }

    std::string buildLevelIntroHint(const LevelConfiguration &levelConfiguration)
    {
        if (levelConfiguration.levelNumber == 1)
        {
            return "Normal enemies are the baseline threat.\n"
                   "They advance in formation, so clear lanes before they reach you.\n"
                   "Use A/D or arrows to move, Space to cast, and Q/E or Shift+move to dash.";
        }

        if (levelConfiguration.levelNumber == 2)
        {
            return "Shielded enemies can block your fireballs.\n"
                   "Dash through them when shots are not enough.\n"
                   "Breaking shields opens the wave back up.";
        }

        if (levelConfiguration.levelNumber == 3)
        {
            return "Shooters fire green projectiles toward where you were standing.\n"
                   "Agile enemies move faster and punish slow lane changes.\n"
                   "Keep moving after a shooter starts its attack.";
        }

        if (levelConfiguration.levelNumber == 4)
        {
            return "Waves can vary in size and enemy combinations.\n"
                   "Some waves are wide, some are dense, and some hide shooters behind blockers.\n"
                   "Pick targets by danger, not just by who is closest.";
        }

        if (levelConfiguration.levelNumber == 5)
        {
            return "This level introduces the first mini boss.\n"
                   "There are 15 total levels in the run: 2 mini bosses and 1 final boss.\n"
                   "Boss projectiles use stronger patterns, so watch the gaps before moving.";
        }

        if (levelConfiguration.levelNumber == 6)
        {
            return "The formations stop being pure basics here. Clear the shooters before the pack closes in.";
        }

        if (levelConfiguration.levelNumber == 7)
        {
            return "Crossfire and flankers arrive together. Save dash for the lane that collapses first.";
        }

        if (levelConfiguration.levelNumber == 8)
        {
            return "Shield anchors hold the center while agile enemies stretch the edges.";
        }

        if (levelConfiguration.levelNumber == 9)
        {
            return "This push mixes density with ranged pressure. Kill order matters more than speed.";
        }

        if (levelConfiguration.levelNumber == 11)
        {
            return "Late-game waves start layering shooters behind shields. Break the spine of the formation first.";
        }

        if (levelConfiguration.levelNumber == 12)
        {
            return "This breakthrough collapses from several lanes at once. Commit to one escape route early.";
        }

        if (levelConfiguration.levelNumber == 13)
        {
            return "Iron Curtain is a wall of guards with firing windows behind it. Dash has to create your openings.";
        }

        if (levelConfiguration.levelNumber == 14)
        {
            return "Last Defense is the dress rehearsal for the boss: dense, fast, and full of bad target choices.";
        }

        if (levelConfiguration.levelContainsMiniBoss)
        {
            return "Boss encounter. Watch the telegraph, then move through the gaps.";
        }

        if (levelConfiguration.levelContainsFinalBoss)
        {
            return "Final exam. Patterns are denser and mistakes hurt more.";
        }

        return "Clear the formation quickly to earn speed and no-hit bonuses.";
    }

    std::string buildShopIntroText()
    {
        return "Between levels, the Upgrade Shop lets you spend essence on upgrades.\n"
               "Essence is the blue currency earned from defeating enemies, clearing levels,\n"
               "and hitting performance bonuses like clean play or fast clears.\n\n"
               "Shop offers can improve health, projectile damage, dash power, scoring,\n"
               "or special effects. You can buy offers, reroll the shop, trade in your\n"
               "latest upgrade, or skip the shop to gain interest and heal.\n\n"
               "Press Enter or click Continue when you are ready for Level 1.";
    }

    std::string buildLevelStatsMessage(const LevelStats &levelStats)
    {
        return "Level Clear\n"
               "Accuracy " +
               std::to_string(UpgradeShop::getAccuracyPercentage(levelStats)) + "%\n"
                                                                                "Shots " +
               std::to_string(levelStats.shotsHit) + "/" +
               std::to_string(levelStats.shotsFired) + "\n"
                                                       "Dash Kills " +
               std::to_string(levelStats.dashKills) + "\n"
                                                      "Near Misses " +
               std::to_string(levelStats.nearMisses) + "\n"
                                                       "Damage Taken " +
               std::to_string(levelStats.damageTaken) + "\n"
                                                        "Max Combo " +
               std::to_string(levelStats.maxCombo) + "\n"
                                                     "Score Earned " +
               std::to_string(levelStats.scoreEarned) + "\n"
                                                        "Essence Earned " +
               std::to_string(levelStats.essenceEarned) + "\n"
                                                          "Clear Time " +
               std::to_string(static_cast<int>(std::round(levelStats.elapsedSeconds))) + "s\n"
                                                                                         "Press Enter for shop";
    }

    sf::FloatRect getOverlayPanelBounds()
    {
        return UpgradeShop::getPanelBounds();
    }

    sf::FloatRect getOverlayContinueButtonBounds()
    {
        return UpgradeShop::getContinueButtonBounds();
    }

    bool containsPoint(const sf::FloatRect &bounds, sf::Vector2f point)
    {
        return point.x >= bounds.position.x &&
               point.x <= bounds.position.x + bounds.size.x &&
               point.y >= bounds.position.y &&
               point.y <= bounds.position.y + bounds.size.y;
    }

    void drawOverlayButton(
        sf::RenderTarget &target,
        const sf::Font &font,
        const OverlayButton &overlayButton)
    {
        sf::RectangleShape buttonShape(overlayButton.bounds.size);
        buttonShape.setPosition(overlayButton.bounds.position);
        buttonShape.setFillColor(
            overlayButton.enabled ? sf::Color(42, 50, 70, 225) : sf::Color(30, 32, 36, 215));
        buttonShape.setOutlineThickness(2.0f);
        buttonShape.setOutlineColor(
            overlayButton.enabled ? sf::Color(145, 210, 255) : sf::Color(90, 90, 90));
        target.draw(buttonShape);

        sf::Text titleText(font);
        titleText.setString(overlayButton.title);
        titleText.setCharacterSize(22);
        titleText.setFillColor(overlayButton.enabled ? sf::Color::White : sf::Color(170, 170, 170));
        titleText.setPosition(overlayButton.bounds.position + sf::Vector2f{18.0f, 10.0f});
        target.draw(titleText);

        sf::Text subtitleText(font);
        subtitleText.setString(overlayButton.subtitle);
        subtitleText.setCharacterSize(16);
        subtitleText.setFillColor(
            overlayButton.enabled ? sf::Color(205, 220, 235) : sf::Color(130, 130, 130));
        subtitleText.setPosition(overlayButton.bounds.position + sf::Vector2f{18.0f, 38.0f});
        target.draw(subtitleText);
    }

    void drawOverlayPanel(
        sf::RenderWindow &window,
        const sf::Font &font,
        const std::vector<LevelConfiguration> &levelConfigurationList,
        int currentLevelIndex,
        LevelProgressState levelProgressState,
        const RunProgress &runProgress,
        int playerEssence)
    {
        sf::RectangleShape backdrop(
            sf::Vector2f{
                static_cast<float>(window.getSize().x),
                static_cast<float>(window.getSize().y),
            });
        backdrop.setFillColor(sf::Color(8, 12, 22, 170));
        window.draw(backdrop);

        const sf::FloatRect panelBounds = getOverlayPanelBounds();
        sf::RectangleShape panelShape(panelBounds.size);
        panelShape.setPosition(panelBounds.position);
        panelShape.setFillColor(sf::Color(18, 22, 34, 238));
        panelShape.setOutlineThickness(3.0f);
        panelShape.setOutlineColor(sf::Color(120, 185, 220));
        window.draw(panelShape);

        const LevelConfiguration &currentLevelConfiguration =
            levelConfigurationList[currentLevelIndex];
        std::string levelHeaderText =
            "Level " + std::to_string(currentLevelConfiguration.levelNumber) +
            ": " + currentLevelConfiguration.levelDisplayName;
        if (currentLevelConfiguration.levelContainsFinalBoss)
        {
            levelHeaderText += " | Final Boss";
        }
        else if (currentLevelConfiguration.levelContainsMiniBoss)
        {
            levelHeaderText += " | Mini Boss";
        }

        sf::Text headerText(font);
        headerText.setCharacterSize(30);
        headerText.setFillColor(sf::Color::White);
        headerText.setPosition({panelBounds.position.x + 28.0f, panelBounds.position.y + 22.0f});

        sf::Text bodyText(font);
        bodyText.setCharacterSize(19);
        bodyText.setFillColor(sf::Color(220, 230, 240));
        bodyText.setPosition({panelBounds.position.x + 30.0f, panelBounds.position.y + 72.0f});

        if (levelProgressState == LevelProgressState::ShowingLevelStats)
        {
            headerText.setString("Level Clear");
            bodyText.setString(
                "Accuracy: " + std::to_string(UpgradeShop::getAccuracyPercentage(runProgress.completedLevelStats)) + "%\n" +
                "Shots Hit: " + std::to_string(runProgress.completedLevelStats.shotsHit) + "/" +
                std::to_string(runProgress.completedLevelStats.shotsFired) + "\n" +
                "Dash Kills: " + std::to_string(runProgress.completedLevelStats.dashKills) + "\n" +
                "Near Misses: " + std::to_string(runProgress.completedLevelStats.nearMisses) + "\n" +
                "Damage Taken: " + std::to_string(runProgress.completedLevelStats.damageTaken) + "\n" +
                "Max Combo: " + std::to_string(runProgress.completedLevelStats.maxCombo) + "\n" +
                "Score Earned: " + std::to_string(runProgress.completedLevelStats.scoreEarned) + "\n" +
                "Essence Earned: " + std::to_string(runProgress.completedLevelStats.essenceEarned) + "\n" +
                "Clear Time: " +
                std::to_string(
                    static_cast<int>(std::round(runProgress.completedLevelStats.elapsedSeconds))) +
                "s");
            window.draw(headerText);
            window.draw(bodyText);
            drawOverlayButton(
                window,
                font,
                {
                    getOverlayContinueButtonBounds(),
                    "Open Shop",
                    "Review upgrades before the next level.",
                    true,
                });
            return;
        }

        if (levelProgressState == LevelProgressState::ShowingShopIntro)
        {
            headerText.setString("Shop and Essence");
            bodyText.setString(buildShopIntroText());
            window.draw(headerText);
            window.draw(bodyText);
            drawOverlayButton(
                window,
                font,
                {
                    getOverlayContinueButtonBounds(),
                    "Continue",
                    "Go to the first level briefing.",
                    true,
                });
            return;
        }

        if (levelProgressState == LevelProgressState::ShowingShop)
        {
            UpgradeShop::drawShopOverlay(
                window,
                font,
                runProgress.shopState,
                playerEssence);
            return;
        }

        if (levelProgressState == LevelProgressState::ShowingLevelIntro)
        {
            headerText.setString(levelHeaderText);
            bodyText.setString(
                buildLevelIntroHint(currentLevelConfiguration) +
                "\n\nPress Enter or click Ready.");
            window.draw(headerText);
            window.draw(bodyText);
            drawOverlayButton(
                window,
                font,
                {
                    getOverlayContinueButtonBounds(),
                    "Ready",
                    "Start the encounter now.",
                    true,
                });
            return;
        }

        headerText.setString(
            levelProgressState == LevelProgressState::PlayerDefeated ? "Defeated" : "Victory");
        bodyText.setString(
            levelProgressState == LevelProgressState::PlayerDefeated
                ? "You were defeated on Level " +
                      std::to_string(currentLevelConfiguration.levelNumber) +
                      ".\nPress Enter or click Retry."
                : "You beat all levels!\nPress Esc to exit.");
        window.draw(headerText);
        window.draw(bodyText);

        if (levelProgressState == LevelProgressState::PlayerDefeated)
        {
            drawOverlayButton(
                window,
                font,
                {
                    getOverlayContinueButtonBounds(),
                    "Retry",
                    "Restart this level with your current upgrades.",
                    true,
                });
        }
    }

    void drawHudInfoBox(
        sf::RenderTarget &target,
        const sf::Font &font,
        const sf::FloatRect &bounds,
        const std::string &label,
        const std::string &value,
        sf::Color accentColor)
    {
        sf::RectangleShape box(bounds.size);
        box.setPosition(bounds.position);
        box.setFillColor(sf::Color(14, 18, 28, 220));
        box.setOutlineThickness(2.0f);
        box.setOutlineColor(accentColor);
        target.draw(box);

        sf::Text labelText(font);
        labelText.setString(label);
        labelText.setCharacterSize(10);
        labelText.setFillColor(sf::Color(165, 185, 210));
        labelText.setPosition(bounds.position + sf::Vector2f{8.0f, 4.0f});
        target.draw(labelText);

        sf::Text valueText(font);
        valueText.setString(value);
        valueText.setCharacterSize(15);
        valueText.setFillColor(sf::Color::White);
        valueText.setPosition(bounds.position + sf::Vector2f{8.0f, 17.0f});
        target.draw(valueText);
    }

    const Enemy *findPrimaryBoss(const std::vector<Enemy> &enemies)
    {
        for (const Enemy &enemy : enemies)
        {
            if (!enemy.isAlive())
            {
                continue;
            }

            if (enemy.getEnemyType() == Enemy::EnemyType::FinalBoss)
            {
                return &enemy;
            }
        }

        for (const Enemy &enemy : enemies)
        {
            if (!enemy.isAlive())
            {
                continue;
            }

            if (enemy.getEnemyType() == Enemy::EnemyType::MiniBoss)
            {
                return &enemy;
            }
        }

        return nullptr;
    }

    void drawBossHealthBar(
        sf::RenderTarget &target,
        const sf::Font &font,
        const Enemy &boss,
        const LevelConfiguration &levelConfiguration)
    {
        const sf::FloatRect bounds = {{104.0f, 106.0f}, {560.0f, 22.0f}};
        sf::RectangleShape barBackground(bounds.size);
        barBackground.setPosition(bounds.position);
        barBackground.setFillColor(sf::Color(30, 18, 22, 230));
        barBackground.setOutlineThickness(2.0f);
        barBackground.setOutlineColor(sf::Color(235, 125, 125));
        target.draw(barBackground);

        const float healthRatio = getEnemyHealthRatio(boss);
        sf::RectangleShape barFill({bounds.size.x * healthRatio, bounds.size.y});
        barFill.setPosition(bounds.position);
        barFill.setFillColor(
            boss.getEnemyType() == Enemy::EnemyType::FinalBoss
                ? sf::Color(255, 95, 95)
                : sf::Color(255, 170, 85));
        target.draw(barFill);

        const int segmentCount = std::clamp(boss.getMaxHealth() / 12, 8, 24);
        for (int segmentIndex = 1; segmentIndex < segmentCount; ++segmentIndex)
        {
            const float x =
                bounds.position.x +
                bounds.size.x * (static_cast<float>(segmentIndex) / static_cast<float>(segmentCount));
            sf::RectangleShape divider({2.0f, bounds.size.y});
            divider.setPosition({x, bounds.position.y});
            divider.setFillColor(sf::Color(55, 20, 20, 180));
            target.draw(divider);
        }

        sf::Text labelText(font);
        labelText.setString(
            buildLevelBannerText(levelConfiguration) + "  " +
            std::to_string(boss.getHealth()) + "/" + std::to_string(boss.getMaxHealth()));
        labelText.setCharacterSize(16);
        labelText.setFillColor(sf::Color::White);
        labelText.setPosition({bounds.position.x, bounds.position.y - 24.0f});
        target.draw(labelText);
    }

    void drawTopHudBoxes(
        sf::RenderTarget &target,
        const sf::Font &font,
        const LevelConfiguration &levelConfiguration,
        const Player &player,
        int score,
        int essence,
        const LevelStats &levelStats,
        const RunCodeState &runCodeState)
    {
        constexpr float boxWidth = 170.0f;
        constexpr float boxHeight = 36.0f;
        constexpr float leftMargin = 12.0f;
        constexpr float topMargin = 8.0f;
        constexpr float horizontalGap = 8.0f;
        constexpr float verticalGap = 4.0f;

        drawHudInfoBox(
            target,
            font,
            {{leftMargin, topMargin}, {boxWidth, boxHeight}},
            "LEVEL",
            buildLevelBannerText(levelConfiguration),
            sf::Color(110, 175, 240));
        drawHudInfoBox(
            target,
            font,
            {{leftMargin + (boxWidth + horizontalGap), topMargin}, {boxWidth, boxHeight}},
            "HEALTH",
            std::to_string(player.getHealth()) + " / " + std::to_string(player.getMaxHealth()),
            sf::Color(125, 220, 160));
        drawHudInfoBox(
            target,
            font,
            {{leftMargin + (boxWidth + horizontalGap) * 2.0f, topMargin}, {boxWidth, boxHeight}},
            "SCORE",
            std::to_string(score),
            sf::Color(255, 205, 105));
        drawHudInfoBox(
            target,
            font,
            {{leftMargin + (boxWidth + horizontalGap) * 3.0f, topMargin}, {boxWidth, boxHeight}},
            "ESSENCE",
            std::to_string(essence),
            sf::Color(170, 210, 255));

        drawHudInfoBox(
            target,
            font,
            {{leftMargin, topMargin + boxHeight + verticalGap}, {boxWidth, boxHeight}},
            "COMBO",
            "Now " + std::to_string(levelStats.currentCombo) +
                "  Best " + std::to_string(levelStats.maxCombo),
            sf::Color(255, 160, 120));
        drawHudInfoBox(
            target,
            font,
            {{leftMargin + (boxWidth + horizontalGap), topMargin + boxHeight + verticalGap}, {boxWidth, boxHeight}},
            "DASH",
            player.canDash() ? "Ready" : "Cooling Down",
            sf::Color(130, 220, 255));
        drawHudInfoBox(
            target,
            font,
            {{leftMargin + (boxWidth + horizontalGap) * 2.0f, topMargin + boxHeight + verticalGap}, {boxWidth, boxHeight}},
            "DEBUG",
            runCodeState.devModeEnabled ? "N skips levels" : "Off",
            sf::Color(205, 170, 255));

        const std::vector<std::string> activeCodeLabels = buildActiveCodeLabels(runCodeState);
        drawHudInfoBox(
            target,
            font,
            {{leftMargin + (boxWidth + horizontalGap) * 3.0f, topMargin + boxHeight + verticalGap}, {boxWidth, boxHeight}},
            "CODES",
            activeCodeLabels.empty()
                ? "None"
                : activeCodeLabels.front() +
                      (activeCodeLabels.size() > 1 ? " +" + std::to_string(activeCodeLabels.size() - 1) : ""),
            sf::Color(255, 145, 190));
    }

    TitleMenuSelection getNextTitleMenuSelection(
        TitleMenuSelection currentSelection,
        int direction)
    {
        const int currentIndex = static_cast<int>(currentSelection);
        const int nextIndex = std::clamp(currentIndex + direction, 0, 2);
        return static_cast<TitleMenuSelection>(nextIndex);
    }

    sf::FloatRect getTitleButtonBounds(TitleMenuSelection selection)
    {
        const float width = 420.0f;
        const float height = 76.0f;
        const float x = 174.0f;
        const float startY = 340.0f;
        const float verticalGap = 18.0f;
        const int index = static_cast<int>(selection);
        return {{x, startY + (height + verticalGap) * static_cast<float>(index)}, {width, height}};
    }

    void drawTitleScreen(
        sf::RenderWindow &window,
        const sf::Font &font,
        TitleMenuSelection titleMenuSelection,
        const RunCodeState &runCodeState)
    {
        sf::RectangleShape background(
            sf::Vector2f{
                static_cast<float>(window.getSize().x),
                static_cast<float>(window.getSize().y),
            });
        background.setFillColor(sf::Color(7, 11, 20));
        window.draw(background);

        sf::Text titleText(font);
        titleText.setString("ASSAIL");
        titleText.setCharacterSize(64);
        titleText.setFillColor(sf::Color::White);
        centerTextHorizontally(titleText, static_cast<float>(window.getSize().x), 120.0f);
        window.draw(titleText);

        sf::Text subtitleText(font);
        subtitleText.setString("Arcade boss rush with unlockable modifier codes");
        subtitleText.setCharacterSize(22);
        subtitleText.setFillColor(sf::Color(180, 205, 225));
        centerTextHorizontally(subtitleText, static_cast<float>(window.getSize().x), 198.0f);
        window.draw(subtitleText);

        drawOverlayButton(
            window,
            font,
            {
                getTitleButtonBounds(TitleMenuSelection::Start),
                "Start",
                "Begin a fresh run.",
                true,
            });
        drawOverlayButton(
            window,
            font,
            {
                getTitleButtonBounds(TitleMenuSelection::EnterCode),
                "Enter Code",
                "Unlock weird run modifiers.",
                true,
            });
        drawOverlayButton(
            window,
            font,
            {
                getTitleButtonBounds(TitleMenuSelection::Exit),
                "Exit",
                "Close the game.",
                true,
            });

        sf::RectangleShape selectionOutline(getTitleButtonBounds(titleMenuSelection).size);
        selectionOutline.setPosition(getTitleButtonBounds(titleMenuSelection).position);
        selectionOutline.setFillColor(sf::Color::Transparent);
        selectionOutline.setOutlineThickness(3.0f);
        selectionOutline.setOutlineColor(sf::Color(255, 214, 120));
        window.draw(selectionOutline);

        const sf::FloatRect guideBounds = {{92.0f, 634.0f}, {584.0f, 204.0f}};
        sf::RectangleShape guidePanel(guideBounds.size);
        guidePanel.setPosition(guideBounds.position);
        guidePanel.setFillColor(sf::Color(13, 18, 30, 225));
        guidePanel.setOutlineThickness(2.0f);
        guidePanel.setOutlineColor(sf::Color(105, 175, 220));
        window.draw(guidePanel);

        sf::Text guideHeaderText(font);
        guideHeaderText.setString("How to Play");
        guideHeaderText.setCharacterSize(24);
        guideHeaderText.setFillColor(sf::Color::White);
        guideHeaderText.setPosition(guideBounds.position + sf::Vector2f{20.0f, 14.0f});
        window.draw(guideHeaderText);

        sf::Text guideText(font);
        guideText.setString(
            "A/D or Arrow Keys: change lanes\n"
            "Space or Right Click: cast fireball\n"
            "Q/E or Shift + move: dash through danger\n"
            "Defeat enemies for score and essence.\n"
            "Spend essence in shops between levels.");
        guideText.setCharacterSize(18);
        guideText.setFillColor(sf::Color(205, 220, 235));
        guideText.setPosition(guideBounds.position + sf::Vector2f{20.0f, 54.0f});
        window.draw(guideText);

        const std::vector<std::string> activeCodeLabels = buildActiveCodeLabels(runCodeState);
        sf::Text codeText(font);
        codeText.setString(
            "Active Codes: " +
            (activeCodeLabels.empty()
                 ? std::string("None")
                 : activeCodeLabels.front() +
                       (activeCodeLabels.size() > 1 ? ", " + activeCodeLabels.back() : "")) +
            "\n" + runCodeState.titleStatusMessage +
            "\nUse Up/Down and Enter, or click a box.");
        codeText.setCharacterSize(18);
        codeText.setFillColor(sf::Color(205, 220, 235));
        codeText.setPosition({106.0f, 864.0f});
        window.draw(codeText);
    }

    void drawCodeEntryScreen(
        sf::RenderWindow &window,
        const sf::Font &font,
        const RunCodeState &runCodeState)
    {
        sf::RectangleShape background(
            sf::Vector2f{
                static_cast<float>(window.getSize().x),
                static_cast<float>(window.getSize().y),
            });
        background.setFillColor(sf::Color(9, 12, 24));
        window.draw(background);

        const sf::FloatRect panelBounds = {{94.0f, 250.0f}, {580.0f, 290.0f}};
        sf::RectangleShape panel(panelBounds.size);
        panel.setPosition(panelBounds.position);
        panel.setFillColor(sf::Color(18, 22, 34, 240));
        panel.setOutlineThickness(3.0f);
        panel.setOutlineColor(sf::Color(120, 185, 220));
        window.draw(panel);

        sf::Text header(font);
        header.setString("Enter Code");
        header.setCharacterSize(34);
        header.setFillColor(sf::Color::White);
        header.setPosition(panelBounds.position + sf::Vector2f{24.0f, 24.0f});
        window.draw(header);

        sf::RectangleShape inputBox({panelBounds.size.x - 48.0f, 64.0f});
        inputBox.setPosition(panelBounds.position + sf::Vector2f{24.0f, 92.0f});
        inputBox.setFillColor(sf::Color(10, 14, 24, 255));
        inputBox.setOutlineThickness(2.0f);
        inputBox.setOutlineColor(sf::Color(145, 210, 255));
        window.draw(inputBox);

        sf::Text inputText(font);
        inputText.setString(runCodeState.codeEntryBuffer.empty() ? "Type here..." : runCodeState.codeEntryBuffer);
        inputText.setCharacterSize(28);
        inputText.setFillColor(
            runCodeState.codeEntryBuffer.empty() ? sf::Color(110, 120, 140) : sf::Color::White);
        inputText.setPosition(inputBox.getPosition() + sf::Vector2f{16.0f, 14.0f});
        window.draw(inputText);

        sf::Text hintText(font);
        hintText.setString(
            "Known codes: DevMode, EmotionalDamage\nEnter to apply. Esc to return.");
        hintText.setCharacterSize(20);
        hintText.setFillColor(sf::Color(195, 210, 228));
        hintText.setPosition(panelBounds.position + sf::Vector2f{24.0f, 182.0f});
        window.draw(hintText);

        sf::Text statusText(font);
        statusText.setString(
            runCodeState.codeEntryStatusMessage.empty()
                ? "Codes persist until you close the game."
                : runCodeState.codeEntryStatusMessage);
        statusText.setCharacterSize(20);
        statusText.setFillColor(sf::Color(255, 215, 130));
        statusText.setPosition(panelBounds.position + sf::Vector2f{24.0f, 242.0f});
        window.draw(statusText);
    }

    void registerEnemyDefeatForCombo(LevelStats &levelStats)
    {
        levelStats.currentCombo += 1;
        levelStats.comboGraceSecondsRemaining = 2.4f;
        levelStats.maxCombo = std::max(levelStats.maxCombo, levelStats.currentCombo);
    }

    void updateLevelStatsTimers(LevelStats &levelStats, float deltaTime)
    {
        levelStats.elapsedSeconds += deltaTime;
        levelStats.comboGraceSecondsRemaining =
            std::max(0.0f, levelStats.comboGraceSecondsRemaining - deltaTime);
        if (levelStats.comboGraceSecondsRemaining <= 0.0f)
        {
            levelStats.currentCombo = 0;
        }
    }

    void awardLevelClearBonuses(
        LevelStats &levelStats,
        int &playerScore,
        int &playerEssence)
    {
        int bonusScore = 0;
        int bonusEssence = 0;

        if (!levelStats.tookDamage)
        {
            bonusScore += 300;
            bonusEssence += 2;
        }

        if (levelStats.elapsedSeconds <= 28.0f)
        {
            bonusScore += 200;
            bonusEssence += 1;
        }

        if (levelStats.maxCombo >= 5)
        {
            bonusScore += levelStats.maxCombo * 20;
        }

        levelStats.scoreEarned += bonusScore;
        levelStats.essenceEarned += bonusEssence;
        playerScore += bonusScore;
        playerEssence += bonusEssence;
    }

    FloatingHudRewardText createFloatingRewardText(
        const sf::Font &font,
        const std::string &message,
        sf::Vector2f position,
        unsigned int characterSize,
        sf::Color color,
        sf::Vector2f velocity = {0.0f, -52.0f},
        float totalLifetimeSeconds = 0.85f)
    {
        FloatingHudRewardText rewardText(font);
        rewardText.text.setString(message);
        rewardText.text.setCharacterSize(characterSize);
        rewardText.text.setFillColor(color);
        const sf::FloatRect bounds = rewardText.text.getLocalBounds();
        rewardText.text.setOrigin({
            bounds.position.x + bounds.size.x * 0.5f,
            bounds.position.y + bounds.size.y * 0.5f,
        });
        rewardText.text.setPosition(position);
        rewardText.velocity = velocity;
        rewardText.totalLifetimeSeconds = totalLifetimeSeconds;
        rewardText.lifetimeSeconds = totalLifetimeSeconds;
        return rewardText;
    }

    // All level design data lives here. Expanding the game mostly means adding new entries
    // or changing row data, instead of rewriting the update loop.
    std::vector<LevelConfiguration> createLevelConfigurationList()
    {
        return {
            {
                1,
                "Opening Steps",
                AssailSong1TrackId,
                1.0f,
                0.0f,
                LevelEncounterStyle::MovingFormation,
                {
                    // Level 1 stays intentionally quiet: one half-row of basic enemies so the
                    // player can learn movement and rhythm before special rules appear.
                    createRepeatedEnemyRow(4, Enemy::EnemyBehaviorProfile::Basic, Enemy::EnemyType::BasicGrunt, 38.0f),
                },
                false,
                false,
            },
            {
                2,
                "Block And Answer",
                AssailSong1TrackId,
                1.0f,
                0.0f,
                LevelEncounterStyle::MovingFormation,
                {
                    // Three quarters of a row introduces shields. The center blockers soak
                    // shots, while the outer basics hint that target order matters.
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                        },
                        34.0f),
                },
                false,
                false,
            },
            {
                3,
                "Crossfire Lesson",
                AssailSong1TrackId,
                1.0f,
                0.0f,
                LevelEncounterStyle::MovingFormation,
                {
                    // Full row: agile enemies slide faster inside the formation while two
                    // shooters telegraph ranged attacks. This level teaches dodging and
                    // encourages dash use without overwhelming the player with shields too.
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                        },
                        28.0f),
                },
                false,
                false,
            },
            {
                4,
                "Mixed Trial",
                AssailSong1TrackId,
                1.0f,
                0.0f,
                LevelEncounterStyle::MovingFormation,
                {
                    // Level 4 is the exam: still a full row overall, but now the player sees
                    // the normal flow with just a few shielded and shooting enemies mixed in.
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                        },
                        34.0f),
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                        },
                        34.0f),
                },
                false,
                false,
            },
            {
                5,
                "Mini Boss Alpha",
                NoBackgroundMusicTrackId,
                1.65f,
                1.25f,
                LevelEncounterStyle::StationaryBossEncounter,
                {
                    createRepeatedEnemyRow(1, Enemy::EnemyBehaviorProfile::Shooter, Enemy::EnemyType::MiniBoss, 0.0f),
                    createRepeatedEnemyRow(4, Enemy::EnemyBehaviorProfile::Basic, Enemy::EnemyType::BasicGrunt, 40.0f),
                },
                true,
                false,
            },
            {
                6,
                "Second Wave",
                NoBackgroundMusicTrackId,
                1.0f,
                0.0f,
                LevelEncounterStyle::MovingFormation,
                {
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                        },
                        34.0f),
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                        },
                        34.0f),
                },
                false,
                false,
            },
            {
                7,
                "Crossfire March",
                NoBackgroundMusicTrackId,
                1.0f,
                0.0f,
                LevelEncounterStyle::MovingFormation,
                {
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                        },
                        34.0f),
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                        },
                        34.0f),
                },
                false,
                false,
            },
            {
                8,
                "Column Collapse",
                NoBackgroundMusicTrackId,
                1.0f,
                0.0f,
                LevelEncounterStyle::MovingFormation,
                {
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                        },
                        34.0f),
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                        },
                        34.0f),
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                        },
                        34.0f),
                },
                false,
                false,
            },
            {
                9,
                "Advance Line",
                NoBackgroundMusicTrackId,
                1.0f,
                0.0f,
                LevelEncounterStyle::MovingFormation,
                {
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                        },
                        34.0f),
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                        },
                        34.0f),
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                        },
                        34.0f),
                },
                false,
                false,
            },
            {
                10,
                "Mini Boss Beta",
                NoBackgroundMusicTrackId,
                1.0f,
                1.5f,
                LevelEncounterStyle::StationaryBossEncounter,
                {
                    createRepeatedEnemyRow(1, Enemy::EnemyBehaviorProfile::Shooter, Enemy::EnemyType::MiniBoss, 0.0f),
                    createRepeatedEnemyRow(3, Enemy::EnemyBehaviorProfile::Basic, Enemy::EnemyType::BasicGrunt, 44.0f),
                },
                true,
                false,
            },
            {
                11,
                "Final Stretch",
                NoBackgroundMusicTrackId,
                1.0f,
                0.0f,
                LevelEncounterStyle::MovingFormation,
                {
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                        },
                        34.0f),
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                        },
                        34.0f),
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                        },
                        34.0f),
                },
                false,
                false,
            },
            {
                12,
                "Breakthrough",
                NoBackgroundMusicTrackId,
                1.0f,
                0.0f,
                LevelEncounterStyle::MovingFormation,
                {
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                        },
                        34.0f),
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                        },
                        34.0f),
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                        },
                        34.0f),
                },
                false,
                false,
            },
            {
                13,
                "Iron Curtain",
                NoBackgroundMusicTrackId,
                1.0f,
                0.0f,
                LevelEncounterStyle::MovingFormation,
                {
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                        },
                        34.0f),
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                        },
                        34.0f),
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                        },
                        34.0f),
                },
                false,
                false,
            },
            {
                14,
                "Last Defense",
                NoBackgroundMusicTrackId,
                1.0f,
                0.0f,
                LevelEncounterStyle::MovingFormation,
                {
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                        },
                        34.0f),
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                        },
                        34.0f),
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shielded),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                        },
                        34.0f),
                    createEnemyRow(
                        {
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Basic),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Shooter),
                            createEnemySpawn(Enemy::EnemyBehaviorProfile::Agile),
                        },
                        34.0f),
                },
                false,
                false,
            },
            {
                15,
                "Final Boss",
                NoBackgroundMusicTrackId,
                1.0f,
                2.0f,
                LevelEncounterStyle::StationaryBossEncounter,
                {
                    createRepeatedEnemyRow(1, Enemy::EnemyBehaviorProfile::Shooter, Enemy::EnemyType::FinalBoss, 0.0f),
                    createRepeatedEnemyRow(4, Enemy::EnemyBehaviorProfile::Basic, Enemy::EnemyType::BasicGrunt, 44.0f),
                },
                false,
                true,
            },
        };
    }

    float getAppliedLevelScaleMultiplier(float configuredLevelScaleMultiplier)
    {
        // A configured value of zero means "leave stats alone" so testing stays simple.
        return configuredLevelScaleMultiplier == 0.0f ? 1.0f : configuredLevelScaleMultiplier;
    }

    int calculateScaledEnemyHealth(
        Enemy::EnemyType enemyType,
        float levelScaleMultiplier)
    {
        const int baseEnemyHealth = Enemy::getBaseHealthForEnemyType(enemyType);
        const float appliedScaleMultiplier = getAppliedLevelScaleMultiplier(levelScaleMultiplier);
        const float bossDurabilityMultiplier =
            enemyType == Enemy::EnemyType::FinalBoss
                ? 1.45f
            : enemyType == Enemy::EnemyType::MiniBoss ? 1.25f
                                                      : 1.0f;
        return std::max(
            1,
            static_cast<int>(
                std::round(baseEnemyHealth * appliedScaleMultiplier * bossDurabilityMultiplier)));
    }

    bool loadHudFont(sf::Font &font)
    {
        constexpr std::array fontPaths{
            "/System/Library/Fonts/Supplemental/Arial.ttf",
            "/System/Library/Fonts/Supplemental/Verdana.ttf",
            "/System/Library/Fonts/Supplemental/Trebuchet MS.ttf",
        };

        for (const char *fontPath : fontPaths)
        {
            if (font.openFromFile(fontPath))
            {
                return true;
            }
        }

        return false;
    }

    bool openMusicFromCandidatePaths(
        sf::Music &music,
        const std::array<const char *, 2> &candidatePaths)
    {
        for (const char *candidatePath : candidatePaths)
        {
            if (music.openFromFile(candidatePath))
            {
                return true;
            }
        }

        return false;
    }

    bool startBackgroundMusicTrackById(
        const std::string &trackId,
        sf::Music &backgroundMusic)
    {
        if (trackId == AssailSong1TrackId)
        {
            if (!openMusicFromCandidatePaths(backgroundMusic, AssailSong1MusicPaths))
            {
                return false;
            }

            backgroundMusic.setLooping(false);
            backgroundMusic.play();
            return true;
        }

        return false;
    }

    void syncBackgroundMusicForLevel(
        const LevelConfiguration &levelConfiguration,
        sf::Music &backgroundMusic,
        std::string &activeBackgroundMusicTrackId)
    {
        const std::string &requestedTrackId = levelConfiguration.backgroundMusicTrackId;
        if (requestedTrackId.empty())
        {
            if (backgroundMusic.getStatus() != sf::SoundSource::Status::Stopped)
            {
                backgroundMusic.stop();
            }
            activeBackgroundMusicTrackId.clear();
            return;
        }

        if (activeBackgroundMusicTrackId == requestedTrackId &&
            backgroundMusic.getStatus() != sf::SoundSource::Status::Stopped)
        {
            return;
        }

        if (!startBackgroundMusicTrackById(requestedTrackId, backgroundMusic))
        {
            activeBackgroundMusicTrackId.clear();
            return;
        }

        activeBackgroundMusicTrackId = requestedTrackId;
    }

    void centerTextHorizontally(sf::Text &text, float windowWidth, float yPosition)
    {
        const sf::FloatRect textBounds = text.getLocalBounds();
        text.setPosition({
            (windowWidth - textBounds.size.x) * 0.5f - textBounds.position.x,
            yPosition,
        });
    }

    sf::FloatRect expandedRect(const sf::FloatRect &rect, float padding)
    {
        return {
            {rect.position.x - padding, rect.position.y - padding},
            {rect.size.x + padding * 2.0f, rect.size.y + padding * 2.0f},
        };
    }

    std::vector<Enemy> createEnemiesForLevelConfiguration(const LevelConfiguration &levelConfiguration)
    {
        std::vector<Enemy> spawnedEnemies;
        const float firstRowY =
            levelConfiguration.levelEncounterStyle == LevelEncounterStyle::MovingFormation
                ? MovingFormationStartY
                : StationaryBossEncounterStartY;

        // Each row definition only needs count, spacing, and enemy type.
        // This helper turns that compact level data into centered, scaled enemy instances.
        for (std::size_t rowIndex = 0; rowIndex < levelConfiguration.enemyRowsToSpawn.size(); ++rowIndex)
        {
            const EnemyRowSpawnConfiguration &rowConfiguration =
                levelConfiguration.enemyRowsToSpawn[rowIndex];
            float fullRowWidth = 0.0f;
            for (std::size_t enemyIndex = 0; enemyIndex < rowConfiguration.enemiesInThisRow.size(); ++enemyIndex)
            {
                const auto &enemySpawnConfiguration = rowConfiguration.enemiesInThisRow[enemyIndex];
                fullRowWidth +=
                    Enemy::getSpriteSizeForConfiguration(
                        enemySpawnConfiguration.enemyType,
                        enemySpawnConfiguration.behaviorProfile)
                        .x;
            }
            if (!rowConfiguration.enemiesInThisRow.empty())
            {
                fullRowWidth +=
                    rowConfiguration.horizontalSpacingBetweenEnemies *
                    static_cast<float>(rowConfiguration.enemiesInThisRow.size() - 1);
            }
            const float rowStartX = (Player::PlayAreaWidth - fullRowWidth) * 0.5f;
            const float rowYPosition = firstRowY + VerticalSpacingBetweenRows * static_cast<float>(rowIndex);

            float nextEnemyXPosition = rowStartX;
            for (const auto &enemySpawnConfiguration : rowConfiguration.enemiesInThisRow)
            {
                const sf::Vector2f enemySpriteSize =
                    Enemy::getSpriteSizeForConfiguration(
                        enemySpawnConfiguration.enemyType,
                        enemySpawnConfiguration.behaviorProfile);
                const int scaledEnemyHealth = calculateScaledEnemyHealth(
                    enemySpawnConfiguration.enemyType,
                    levelConfiguration.levelScaleMultiplier);

                spawnedEnemies.emplace_back(
                    sf::Vector2f{nextEnemyXPosition, rowYPosition},
                    enemySpawnConfiguration.enemyType,
                    scaledEnemyHealth,
                    enemySpawnConfiguration.behaviorProfile);
                nextEnemyXPosition +=
                    enemySpriteSize.x + rowConfiguration.horizontalSpacingBetweenEnemies;
            }
        }

        return spawnedEnemies;
    }

    sf::FloatRect getEnemyGroupBounds(const std::vector<Enemy> &enemies)
    {
        sf::FloatRect enemyGroupBounds;
        bool hasAtLeastOneEnemyBounds = false;

        for (const Enemy &enemy : enemies)
        {
            if (!enemy.isAlive())
            {
                continue;
            }

            const sf::FloatRect enemyBounds = enemy.getBounds();
            if (!hasAtLeastOneEnemyBounds)
            {
                enemyGroupBounds = enemyBounds;
                hasAtLeastOneEnemyBounds = true;
                continue;
            }

            const float leftEdge = std::min(enemyGroupBounds.position.x, enemyBounds.position.x);
            const float topEdge = std::min(enemyGroupBounds.position.y, enemyBounds.position.y);
            const float rightEdge = std::max(
                enemyGroupBounds.position.x + enemyGroupBounds.size.x,
                enemyBounds.position.x + enemyBounds.size.x);
            const float bottomEdge = std::max(
                enemyGroupBounds.position.y + enemyGroupBounds.size.y,
                enemyBounds.position.y + enemyBounds.size.y);

            enemyGroupBounds = {{leftEdge, topEdge}, {rightEdge - leftEdge, bottomEdge - topEdge}};
        }

        return enemyGroupBounds;
    }

    void updateMovingFormationEnemies(
        const LevelConfiguration &levelConfiguration,
        std::vector<Enemy> &enemies,
        float deltaTime,
        float difficultyScale,
        float playAreaWidth,
        float &enemyFormationDirection)
    {
        if (levelConfiguration.levelEncounterStyle != LevelEncounterStyle::MovingFormation ||
            enemies.empty())
        {
            return;
        }

        // Standard levels move the full group like Space Invaders.
        const float horizontalStep =
            EnemyFormationHorizontalSpeed * difficultyScale * enemyFormationDirection * deltaTime;
        for (Enemy &enemy : enemies)
        {
            // Agile enemies intentionally slide faster than the pack so the player
            // reads them as a different threat even though the formation still shares
            // one overall direction and wall-bounce rhythm.
            enemy.move({horizontalStep * enemy.getFormationSpeedMultiplier(), 0.0f});
        }

        const sf::FloatRect enemyGroupBounds = getEnemyGroupBounds(enemies);
        const bool formationTouchedLeftWall = enemyGroupBounds.position.x <= 0.0f;
        const bool formationTouchedRightWall =
            enemyGroupBounds.position.x + enemyGroupBounds.size.x >= playAreaWidth;

        if (!formationTouchedLeftWall && !formationTouchedRightWall)
        {
            return;
        }

        const float horizontalCorrection = formationTouchedLeftWall
                                               ? -enemyGroupBounds.position.x
                                               : playAreaWidth - (enemyGroupBounds.position.x + enemyGroupBounds.size.x);

        for (Enemy &enemy : enemies)
        {
            enemy.move({horizontalCorrection, EnemyFormationDropDistance});
        }

        enemyFormationDirection *= -1.0f;
    }

    bool aliveEnemyReachedBottomOfScreen(
        const std::vector<Enemy> &enemies,
        float screenHeight)
    {
        for (const Enemy &enemy : enemies)
        {
            if (!enemy.isAlive())
            {
                continue;
            }

            const sf::FloatRect enemyBounds = enemy.getBounds();
            if (enemyBounds.position.y + enemyBounds.size.y >= screenHeight)
            {
                return true;
            }
        }

        return false;
    }

    void updateEnemyIdleAnimation(std::vector<Enemy> &enemies, float deltaTime)
    {
        for (Enemy &enemy : enemies)
        {
            enemy.updateVisualAnimation(deltaTime);
        }
    }

    void firePlayerProjectiles(
        Player &player,
        std::vector<Projectile> &playerProjectiles,
        float launchDelaySeconds,
        const UpgradeShop::FiredShotPlan &shotPlan,
        RunCodeState &runCodeState)
    {
        // The player casting flow now creates the shot at the hand position first,
        // then the projectile begins moving after the cast animation has finished.
        ++runCodeState.attacksLaunchedThisRun;
        playerProjectiles.emplace_back(
            player.getProjectileSpawnPosition(),
            sf::Vector2f{0.0f, -720.0f},
            shotPlan.projectileDamage,
            5.0f,
            sf::Color::White,
            launchDelaySeconds,
            shotPlan.projectilePierceCount);
        for (int echoProjectileIndex = 0;
             echoProjectileIndex < shotPlan.echoProjectileCount;
             ++echoProjectileIndex)
        {
            const float horizontalOffset = echoProjectileIndex % 2 == 0 ? 14.0f : -14.0f;
            playerProjectiles.emplace_back(
                player.getProjectileSpawnPosition() + sf::Vector2f{horizontalOffset, 0.0f},
                sf::Vector2f{0.0f, -720.0f},
                shotPlan.projectileDamage,
                4.0f,
                sf::Color(170, 225, 255),
                launchDelaySeconds + 0.03f * static_cast<float>(echoProjectileIndex + 1),
                shotPlan.projectilePierceCount);
        }

        if (runCodeState.emotionalDamageEnabled &&
            runCodeState.attacksLaunchedThisRun % 3 == 0)
        {
            constexpr int wallProjectileCount = 9;
            constexpr float wallLeftX = 84.0f;
            constexpr float wallWidth = 600.0f;
            for (int projectileIndex = 0; projectileIndex < wallProjectileCount; ++projectileIndex)
            {
                const float xPosition =
                    wallLeftX +
                    wallWidth *
                        (static_cast<float>(projectileIndex) /
                         static_cast<float>(wallProjectileCount - 1));
                playerProjectiles.emplace_back(
                    sf::Vector2f{xPosition, player.getProjectileSpawnPosition().y},
                    sf::Vector2f{0.0f, -760.0f},
                    shotPlan.projectileDamage + 4,
                    4.5f,
                    sf::Color(255, 160, 210),
                    launchDelaySeconds + 0.02f * static_cast<float>(projectileIndex % 3),
                    shotPlan.projectilePierceCount);
            }
        }
        player.triggerShotRecoil();
    }

    void letProjectileEnemiesFireProjectiles(
        const LevelConfiguration &levelConfiguration,
        std::vector<Enemy> &enemies,
        const Player &player,
        std::vector<Projectile> &enemyProjectiles,
        float difficultyScale,
        float deltaTime,
        FrameCombatFeedback &frameCombatFeedback)
    {
        // Any enemy with a ranged role owns its own cooldown. That keeps shooter logic
        // local to the enemy profile instead of coupling it to one encounter type.
        const sf::Sprite *playerSprite = player.getSprite();
        const sf::FloatRect playerBounds =
            playerSprite ? playerSprite->getGlobalBounds() : sf::FloatRect();
        const sf::Vector2f playerSnapshotPosition = playerSprite
                                                        ? sf::Vector2f{
                                                              playerBounds.position.x + playerBounds.size.x * 0.5f,
                                                              playerBounds.position.y + playerBounds.size.y * 0.5f,
                                                          }
                                                        : player.getPosition();
        for (Enemy &enemy : enemies)
        {
            enemy.updateAttackCooldown(deltaTime);
            if (!enemy.isAlive() || !enemy.isReadyToFireProjectile())
            {
                continue;
            }

            const std::vector<Projectile> spawnedProjectiles =
                createEnemyAttackProjectiles(
                    enemy,
                    levelConfiguration,
                    playerSnapshotPosition,
                    difficultyScale);
            for (const Projectile &projectile : spawnedProjectiles)
            {
                enemyProjectiles.push_back(projectile);
            }
            enemy.resetAttackCooldown(levelConfiguration.enemyProjectileCooldownMultiplier);
            if (enemy.getEnemyType() == Enemy::EnemyType::MiniBoss ||
                enemy.getEnemyType() == Enemy::EnemyType::FinalBoss)
            {
                frameCombatFeedback.bossAttackTriggeredThisFrame = true;
            }
        }
    }

    sf::Vector2f getEnemyCenter(const Enemy &enemy)
    {
        return enemy.getPosition() + enemy.getSpriteSize() * 0.5f;
    }

    float getDistanceSquared(sf::Vector2f firstPoint, sf::Vector2f secondPoint)
    {
        const sf::Vector2f delta = firstPoint - secondPoint;
        return delta.x * delta.x + delta.y * delta.y;
    }

    void awardEnemyDefeat(
        Enemy &enemy,
        FrameCombatFeedback &frameCombatFeedback,
        const sf::Font *hudFont,
        const std::string &prefixText,
        sf::Color effectColor)
    {
        const bool enemyWasBoss =
            enemy.getEnemyType() == Enemy::EnemyType::MiniBoss ||
            enemy.getEnemyType() == Enemy::EnemyType::FinalBoss;
        frameCombatFeedback.scoreEarnedThisFrame +=
            getScoreValueForEnemyType(enemy.getEnemyType());
        frameCombatFeedback.essenceEarnedThisFrame +=
            getEssenceValueForEnemyType(enemy.getEnemyType());
        ++frameCombatFeedback.enemiesDefeatedThisFrame;
        frameCombatFeedback.enemyDeathEffectsToSpawn.emplace_back(
            getEnemyCenter(enemy),
            enemy.getSpriteSize(),
            enemyWasBoss ? sf::Color(255, 170, 70) : effectColor);

        if (!hudFont)
        {
            return;
        }

        const std::string behaviorLabel =
            enemy.getEnemyType() == Enemy::EnemyType::BasicGrunt
                ? getEnemyBehaviorLabel(enemy.getBehaviorProfile()) + "  "
                : "";
        frameCombatFeedback.floatingRewardTextsToSpawn.push_back(
            createFloatingRewardText(
                *hudFont,
                prefixText + behaviorLabel +
                    "+" + std::to_string(getScoreValueForEnemyType(enemy.getEnemyType())) +
                    " score  +" +
                    std::to_string(getEssenceValueForEnemyType(enemy.getEnemyType())) +
                    " essence",
                getEnemyCenter(enemy),
                enemyWasBoss ? 24U : 18U,
                enemy.getEnemyType() == Enemy::EnemyType::BasicGrunt
                    ? getRewardTextColorForEnemyBehavior(enemy.getBehaviorProfile())
                    : getRewardTextColorForEnemyType(enemy.getEnemyType())));
    }

    void damageEnemiesNearPoint(
        std::vector<Enemy> &enemies,
        sf::Vector2f centerPosition,
        float radius,
        int damage,
        FrameCombatFeedback &frameCombatFeedback,
        const sf::Font *hudFont,
        const Enemy *sourceEnemy,
        const std::string &popupText,
        sf::Color effectColor)
    {
        if (damage <= 0)
        {
            return;
        }

        const float radiusSquared = radius * radius;
        for (Enemy &nearbyEnemy : enemies)
        {
            if (!nearbyEnemy.isAlive() || &nearbyEnemy == sourceEnemy)
            {
                continue;
            }

            if (getDistanceSquared(centerPosition, getEnemyCenter(nearbyEnemy)) > radiusSquared)
            {
                continue;
            }

            const Enemy::HitResult splashHitResult =
                nearbyEnemy.applyPlayerProjectileHit(damage);
            if (splashHitResult.enemyTookDamage)
            {
                nearbyEnemy.triggerDamageFlash();
                if (nearbyEnemy.getEnemyType() == Enemy::EnemyType::MiniBoss ||
                    nearbyEnemy.getEnemyType() == Enemy::EnemyType::FinalBoss)
                {
                    frameCombatFeedback.bossTookDamageThisFrame = true;
                }
            }

            if (hudFont && (splashHitResult.enemyTookDamage || splashHitResult.attackWasBlocked))
            {
                frameCombatFeedback.floatingRewardTextsToSpawn.push_back(
                    createFloatingRewardText(
                        *hudFont,
                        splashHitResult.attackWasBlocked ? "Blocked" : popupText,
                        getEnemyCenter(nearbyEnemy),
                        15U,
                        effectColor,
                        {0.0f, -42.0f},
                        0.55f));
            }

            if (splashHitResult.enemyWasDefeated)
            {
                awardEnemyDefeat(
                    nearbyEnemy,
                    frameCombatFeedback,
                    hudFont,
                    popupText + "  ",
                    effectColor);
            }
        }
    }

    void damageEnemiesHitByPlayerProjectiles(
        std::vector<Projectile> &playerProjectiles,
        std::vector<Enemy> &enemies,
        Player &player,
        FrameCombatFeedback &frameCombatFeedback,
        UpgradeShop::PlayerUpgradeState &upgradeState,
        const sf::Font *hudFont)
    {
        for (Projectile &playerProjectile : playerProjectiles)
        {
            if (!playerProjectile.isActive())
            {
                continue;
            }

            for (Enemy &enemy : enemies)
            {
                if (!enemy.isAlive())
                {
                    continue;
                }

                if (playerProjectile.getBounds().findIntersection(enemy.getBounds()).has_value())
                {
                    const bool enemyWasBoss =
                        enemy.getEnemyType() == Enemy::EnemyType::MiniBoss ||
                        enemy.getEnemyType() == Enemy::EnemyType::FinalBoss;
                    const bool enemyWasLowHealthBeforeHit =
                        enemy.getHealth() <=
                        std::max(1, Enemy::getBaseHealthForEnemyType(enemy.getEnemyType()) / 2);
                    const int projectileDamage =
                        playerProjectile.getDamage() +
                        UpgradeShop::calculateProjectileDamageBonusOnHit(
                            enemy,
                            upgradeState);
                    const Enemy::HitResult hitResult =
                        enemy.applyPlayerProjectileHit(projectileDamage);
                    const bool projectilePierced =
                        hitResult.enemyTookDamage &&
                        !hitResult.attackWasBlocked &&
                        playerProjectile.consumePierceCharge();
                    if (projectilePierced)
                    {
                        playerProjectile.nudgeForwardAfterPierce(24.0f);
                    }
                    else
                    {
                        playerProjectile.destroy();
                    }
                    if (hitResult.enemyTookDamage)
                    {
                        enemy.triggerDamageFlash();
                        ++frameCombatFeedback.playerShotsHitThisFrame;
                        if (upgradeState.ricochetHitInterval > 0 &&
                            upgradeState.ricochetDamage > 0)
                        {
                            ++upgradeState.projectileHitsSinceRicochet;
                            if (upgradeState.projectileHitsSinceRicochet >=
                                upgradeState.ricochetHitInterval)
                            {
                                upgradeState.projectileHitsSinceRicochet = 0;
                                Enemy *ricochetTarget = nullptr;
                                float closestDistanceSquared = 0.0f;
                                for (Enemy &candidateEnemy : enemies)
                                {
                                    if (!candidateEnemy.isAlive() || &candidateEnemy == &enemy)
                                    {
                                        continue;
                                    }

                                    const float candidateDistanceSquared =
                                        getDistanceSquared(
                                            getEnemyCenter(enemy),
                                            getEnemyCenter(candidateEnemy));
                                    if (!ricochetTarget ||
                                        candidateDistanceSquared < closestDistanceSquared)
                                    {
                                        ricochetTarget = &candidateEnemy;
                                        closestDistanceSquared = candidateDistanceSquared;
                                    }
                                }

                                if (ricochetTarget)
                                {
                                    const Enemy::HitResult ricochetHitResult =
                                        ricochetTarget->applyPlayerProjectileHit(
                                            upgradeState.ricochetDamage);
                                    if (ricochetHitResult.enemyTookDamage)
                                    {
                                        ricochetTarget->triggerDamageFlash();
                                        if (ricochetTarget->getEnemyType() ==
                                                Enemy::EnemyType::MiniBoss ||
                                            ricochetTarget->getEnemyType() ==
                                                Enemy::EnemyType::FinalBoss)
                                        {
                                            frameCombatFeedback.bossTookDamageThisFrame = true;
                                        }
                                    }
                                    if (hudFont)
                                    {
                                        frameCombatFeedback.floatingRewardTextsToSpawn.push_back(
                                            createFloatingRewardText(
                                                *hudFont,
                                                ricochetHitResult.attackWasBlocked
                                                    ? "Ricochet Blocked"
                                                    : "Ricochet",
                                                getEnemyCenter(*ricochetTarget),
                                                15U,
                                                sf::Color(170, 225, 255),
                                                {0.0f, -42.0f},
                                                0.55f));
                                    }
                                    if (ricochetHitResult.enemyWasDefeated)
                                    {
                                        awardEnemyDefeat(
                                            *ricochetTarget,
                                            frameCombatFeedback,
                                            hudFont,
                                            "Ricochet  ",
                                            sf::Color(170, 225, 255));
                                    }
                                }
                            }
                        }
                        if (enemyWasBoss)
                        {
                            frameCombatFeedback.bossTookDamageThisFrame = true;
                        }
                    }

                    if (hitResult.attackWasBlocked && hudFont)
                    {
                        frameCombatFeedback.floatingRewardTextsToSpawn.push_back(
                            createFloatingRewardText(
                                *hudFont,
                                hitResult.shieldWasBroken ? "Guard Broken" : "Blocked",
                                enemy.getPosition() + enemy.getSpriteSize() * 0.5f,
                                16U,
                                sf::Color(165, 220, 255),
                                {0.0f, -42.0f},
                                0.55f));
                    }

                    if (hitResult.enemyWasDefeated)
                    {
                        awardEnemyDefeat(
                            enemy,
                            frameCombatFeedback,
                            hudFont,
                            "",
                            sf::Color(90, 240, 150));
                        if (upgradeState.vampireHealOnKill > 0)
                        {
                            player.heal(upgradeState.vampireHealOnKill);
                        }
                        if (hudFont && upgradeState.vampireHealOnKill > 0)
                        {
                            frameCombatFeedback.floatingRewardTextsToSpawn.push_back(
                                createFloatingRewardText(
                                    *hudFont,
                                    "Vampire +HP",
                                    getEnemyCenter(enemy) + sf::Vector2f{0.0f, 20.0f},
                                    14U,
                                    sf::Color(255, 120, 150),
                                    {0.0f, -36.0f},
                                    0.5f));
                        }
                        if (enemyWasLowHealthBeforeHit && upgradeState.executionBloomDamage > 0)
                        {
                            damageEnemiesNearPoint(
                                enemies,
                                getEnemyCenter(enemy),
                                130.0f,
                                upgradeState.executionBloomDamage,
                                frameCombatFeedback,
                                hudFont,
                                &enemy,
                                "Bloom",
                                sf::Color(255, 185, 110));
                        }
                    }
                    break;
                }
            }
        }
    }

    void damageEnemiesHitByPlayerDash(
        Player &player,
        std::vector<Enemy> &enemies,
        FrameCombatFeedback &frameCombatFeedback,
        const sf::Font *hudFont)
    {
        // Dash collision is intentionally limited to weak enemies for now so the move
        // feels powerful without letting the player erase bosses for free.
        if (!player.isPerformingDashAttack())
        {
            return;
        }

        const sf::Sprite *playerSprite = player.getSprite();
        if (!playerSprite)
        {
            return;
        }

        const sf::FloatRect playerBounds = playerSprite->getGlobalBounds();
        for (Enemy &enemy : enemies)
        {
            if (!enemy.isAlive() ||
                enemy.getEnemyType() != Enemy::EnemyType::BasicGrunt)
            {
                continue;
            }

            if (!playerBounds.findIntersection(enemy.getBounds()).has_value())
            {
                continue;
            }

            const Enemy::HitResult hitResult = enemy.applyPlayerDashHit(enemy.getHealth());
            if (hitResult.enemyTookDamage)
            {
                enemy.triggerDamageFlash();
            }
            frameCombatFeedback.dashHitEnemyThisFrame = true;
            if (hitResult.enemyWasDefeated)
            {
                // Dash breaks grant a small score bonus over a regular grunt kill to reward riskier play.
                frameCombatFeedback.scoreEarnedThisFrame +=
                    getScoreValueForEnemyType(enemy.getEnemyType()) + 50;
                frameCombatFeedback.essenceEarnedThisFrame +=
                    getEssenceValueForEnemyType(enemy.getEnemyType());
                ++frameCombatFeedback.dashKillsThisFrame;
                ++frameCombatFeedback.enemiesDefeatedThisFrame;
                frameCombatFeedback.enemyDeathEffectsToSpawn.emplace_back(
                    enemy.getPosition() + enemy.getSpriteSize() * 0.5f,
                    enemy.getSpriteSize(),
                    sf::Color(100, 220, 255));
                if (hudFont)
                {
                    const std::string dashRewardLabel =
                        hitResult.shieldWasBroken ? "Dash Break  +" : "Dash Strike  +";
                    frameCombatFeedback.floatingRewardTextsToSpawn.push_back(
                        createFloatingRewardText(
                            *hudFont,
                            dashRewardLabel +
                                std::to_string(getScoreValueForEnemyType(enemy.getEnemyType()) + 50),
                            enemy.getPosition() + enemy.getSpriteSize() * 0.5f,
                            18U,
                            sf::Color(110, 220, 255),
                            {0.0f, -64.0f},
                            0.75f));
                }
            }
            else if (hitResult.shieldWasBroken && hudFont)
            {
                frameCombatFeedback.floatingRewardTextsToSpawn.push_back(
                    createFloatingRewardText(
                        *hudFont,
                        "Shield Cracked",
                        enemy.getPosition() + enemy.getSpriteSize() * 0.5f,
                        16U,
                        sf::Color(150, 225, 255),
                        {0.0f, -52.0f},
                        0.6f));
            }
        }
    }

    void damagePlayerHitByEnemyProjectiles(
        std::vector<Projectile> &enemyProjectiles,
        Player &player,
        FrameCombatFeedback &frameCombatFeedback)
    {
        const sf::Sprite *playerSprite = player.getSprite();
        if (!playerSprite)
        {
            return;
        }

        const sf::FloatRect playerBounds = playerSprite->getGlobalBounds();
        for (Projectile &enemyProjectile : enemyProjectiles)
        {
            if (!enemyProjectile.isActive())
            {
                continue;
            }

            if (enemyProjectile.getBounds().findIntersection(playerBounds).has_value())
            {
                const int healthBeforeHit = player.getHealth();
                player.applyDamageIfVulnerable(enemyProjectile.getDamage());
                enemyProjectile.destroy();
                if (player.getHealth() < healthBeforeHit)
                {
                    frameCombatFeedback.playerTookDamageThisFrame = true;
                    frameCombatFeedback.playerDamageTakenThisFrame +=
                        healthBeforeHit - player.getHealth();
                }
            }
        }
    }

    void collectDashNearMissRewards(
        std::vector<Projectile> &enemyProjectiles,
        Player &player,
        FrameCombatFeedback &frameCombatFeedback,
        const sf::Font *hudFont)
    {
        if (!player.isInvulnerable())
        {
            return;
        }

        const sf::Sprite *playerSprite = player.getSprite();
        if (!playerSprite)
        {
            return;
        }

        const sf::FloatRect playerBounds = playerSprite->getGlobalBounds();
        const sf::FloatRect nearMissBounds = expandedRect(playerBounds, DashNearMissPadding);
        bool earnedRewardThisFrame = false;

        for (Projectile &enemyProjectile : enemyProjectiles)
        {
            if (!enemyProjectile.isActive())
            {
                continue;
            }

            const sf::FloatRect projectileBounds = enemyProjectile.getBounds();
            const bool hitPlayer = projectileBounds.findIntersection(playerBounds).has_value();
            const bool grazedPlayer = projectileBounds.findIntersection(nearMissBounds).has_value();
            if (hitPlayer || !grazedPlayer)
            {
                continue;
            }

            // Near misses are treated like a graze mechanic: the projectile is consumed,
            // the dash comes back sooner, and the player gets a small score reward.
            player.reduceRemainingDashCooldown(DashNearMissCooldownRewardSeconds);
            enemyProjectile.destroy();
            frameCombatFeedback.scoreEarnedThisFrame += 25;
            ++frameCombatFeedback.nearMissesThisFrame;
            earnedRewardThisFrame = true;

            if (hudFont)
            {
                frameCombatFeedback.floatingRewardTextsToSpawn.push_back(
                    createFloatingRewardText(
                        *hudFont,
                        "Near Miss  +25",
                        {
                            projectileBounds.position.x + projectileBounds.size.x * 0.5f,
                            projectileBounds.position.y,
                        },
                        16U,
                        sf::Color(170, 230, 255),
                        {0.0f, -46.0f},
                        0.65f));
            }
        }

        if (earnedRewardThisFrame)
        {
            frameCombatFeedback.dashHitEnemyThisFrame = true;
        }
    }

    void removeDefeatedEnemies(std::vector<Enemy> &enemies)
    {
        enemies.erase(
            std::remove_if(
                enemies.begin(),
                enemies.end(),
                [](const Enemy &enemy)
                {
                    return !enemy.isAlive();
                }),
            enemies.end());
    }

    void removeInactiveProjectiles(std::vector<Projectile> &projectiles)
    {
        projectiles.erase(
            std::remove_if(
                projectiles.begin(),
                projectiles.end(),
                [](const Projectile &projectile)
                {
                    return !projectile.isActive() || projectile.isOffscreen();
                }),
            projectiles.end());
    }

    void updateEnemyDeathEffects(
        std::vector<EnemyDeathEffect> &enemyDeathEffects,
        float deltaTime)
    {
        for (EnemyDeathEffect &enemyDeathEffect : enemyDeathEffects)
        {
            enemyDeathEffect.update(deltaTime);
        }

        enemyDeathEffects.erase(
            std::remove_if(
                enemyDeathEffects.begin(),
                enemyDeathEffects.end(),
                [](const EnemyDeathEffect &enemyDeathEffect)
                {
                    return enemyDeathEffect.isFinished();
                }),
            enemyDeathEffects.end());
    }

    void appendEnemyDeathEffects(
        std::vector<EnemyDeathEffect> &activeEnemyDeathEffects,
        std::vector<EnemyDeathEffect> &newEnemyDeathEffects)
    {
        for (EnemyDeathEffect &newEnemyDeathEffect : newEnemyDeathEffects)
        {
            activeEnemyDeathEffects.push_back(std::move(newEnemyDeathEffect));
        }
    }

    void updateFloatingRewardTexts(
        std::vector<FloatingHudRewardText> &floatingRewardTexts,
        float deltaTime)
    {
        // Floating reward text is updated in world space so it appears anchored to
        // the kill or dodge location before fading away.
        for (FloatingHudRewardText &floatingRewardText : floatingRewardTexts)
        {
            floatingRewardText.lifetimeSeconds =
                std::max(0.0f, floatingRewardText.lifetimeSeconds - deltaTime);
            floatingRewardText.text.move(floatingRewardText.velocity * deltaTime);
            const float lifeRatio =
                floatingRewardText.totalLifetimeSeconds <= 0.0f
                    ? 0.0f
                    : floatingRewardText.lifetimeSeconds /
                          floatingRewardText.totalLifetimeSeconds;
            sf::Color color = floatingRewardText.text.getFillColor();
            color.a = static_cast<std::uint8_t>(255.0f * std::clamp(lifeRatio, 0.0f, 1.0f));
            floatingRewardText.text.setFillColor(color);
        }

        floatingRewardTexts.erase(
            std::remove_if(
                floatingRewardTexts.begin(),
                floatingRewardTexts.end(),
                [](const FloatingHudRewardText &floatingRewardText)
                {
                    return floatingRewardText.lifetimeSeconds <= 0.0f;
                }),
            floatingRewardTexts.end());
    }

    void appendFloatingRewardTexts(
        std::vector<FloatingHudRewardText> &activeFloatingRewardTexts,
        std::vector<FloatingHudRewardText> &newFloatingRewardTexts)
    {
        for (FloatingHudRewardText &newFloatingRewardText : newFloatingRewardTexts)
        {
            activeFloatingRewardTexts.push_back(std::move(newFloatingRewardText));
        }
    }

    void startHitPause(float &remainingHitPauseSeconds, float hitPauseDurationSeconds)
    {
        remainingHitPauseSeconds = std::max(remainingHitPauseSeconds, hitPauseDurationSeconds);
    }

    void resetPlayerForFreshLevelAttempt(Player &player)
    {
        player.setPosition({352.0f, 820.0f});
        player.setHealth(player.getMaxHealth());
        player.syncMovementTarget();
        player.setShooting(false);
    }

    void startLevelAttempt(
        const std::vector<LevelConfiguration> &levelConfigurationList,
        int currentLevelIndex,
        Player &player,
        std::vector<Enemy> &enemies,
        std::vector<Projectile> &playerProjectiles,
        std::vector<Projectile> &enemyProjectiles,
        std::vector<EnemyDeathEffect> &activeEnemyDeathEffects,
        std::vector<FloatingHudRewardText> &activeFloatingRewardTexts,
        ScreenShakeController &screenShakeController,
        float &enemyFormationDirection,
        LevelProgressState &levelProgressState)
    {
        // A fresh level attempt clears only short-lived combat state. Score and essence are
        // intentionally left alone so the player can carry progression across the run.
        resetPlayerForFreshLevelAttempt(player);
        enemies = createEnemiesForLevelConfiguration(levelConfigurationList[currentLevelIndex]);
        playerProjectiles.clear();
        enemyProjectiles.clear();
        activeEnemyDeathEffects.clear();
        activeFloatingRewardTexts.clear();
        screenShakeController.reset();
        enemyFormationDirection = 1.0f;
        levelProgressState = LevelProgressState::Playing;
    }

    void skipToNextLevelForDebugTesting(
        const std::vector<LevelConfiguration> &levelConfigurationList,
        int &currentLevelIndex,
        Player &player,
        std::vector<Enemy> &enemies,
        std::vector<Projectile> &playerProjectiles,
        std::vector<Projectile> &enemyProjectiles,
        std::vector<EnemyDeathEffect> &activeEnemyDeathEffects,
        std::vector<FloatingHudRewardText> &activeFloatingRewardTexts,
        ScreenShakeController &screenShakeController,
        float &enemyFormationDirection,
        LevelProgressState &levelProgressState)
    {
        if (currentLevelIndex >= static_cast<int>(levelConfigurationList.size()) - 1)
        {
            levelProgressState = LevelProgressState::Victory;
            enemies.clear();
            playerProjectiles.clear();
            enemyProjectiles.clear();
            activeEnemyDeathEffects.clear();
            activeFloatingRewardTexts.clear();
            screenShakeController.reset();
            return;
        }

        // Debug shortcut: jump straight to the next level so miniboss and boss fights are easy to test.
        ++currentLevelIndex;
        startLevelAttempt(
            levelConfigurationList,
            currentLevelIndex,
            player,
            enemies,
            playerProjectiles,
            enemyProjectiles,
            activeEnemyDeathEffects,
            activeFloatingRewardTexts,
            screenShakeController,
            enemyFormationDirection,
            levelProgressState);
    }

    std::string buildLevelBannerText(const LevelConfiguration &levelConfiguration)
    {
        std::string levelBannerText =
            "Level " + std::to_string(levelConfiguration.levelNumber) +
            ": " + levelConfiguration.levelDisplayName;

        if (levelConfiguration.levelContainsFinalBoss)
        {
            levelBannerText += " | Final Boss";
        }
        else if (levelConfiguration.levelContainsMiniBoss)
        {
            levelBannerText += " | Mini Boss";
        }

        return levelBannerText;
    }

    std::string buildOverlayMessage(
        const std::vector<LevelConfiguration> &levelConfigurationList,
        int currentLevelIndex,
        LevelProgressState levelProgressState,
        const RunProgress &runProgress,
        int playerEssence)
    {
        const LevelConfiguration &currentLevelConfiguration =
            levelConfigurationList[currentLevelIndex];

        if (levelProgressState == LevelProgressState::Victory)
        {
            return "You beat all levels!\nPress Esc to exit";
        }

        if (levelProgressState == LevelProgressState::PlayerDefeated)
        {
            return "You were defeated on Level " +
                   std::to_string(currentLevelConfiguration.levelNumber) +
                   ".\nPress Enter to retry";
        }

        if (levelProgressState == LevelProgressState::ShowingLevelStats)
        {
            return buildLevelStatsMessage(runProgress.completedLevelStats);
        }

        if (levelProgressState == LevelProgressState::ShowingShop)
        {
            return UpgradeShop::buildShopMessage(runProgress.shopState, playerEssence);
        }

        if (levelProgressState == LevelProgressState::ShowingShopIntro)
        {
            return "Shop and Essence\n" + buildShopIntroText();
        }

        if (levelProgressState == LevelProgressState::ShowingLevelIntro)
        {
            return buildLevelBannerText(currentLevelConfiguration) +
                   "\n" + buildLevelIntroHint(currentLevelConfiguration) +
                   "\nPress Enter to begin";
        }

        return "";
    }

    std::string buildTopHudText(
        const LevelConfiguration &levelConfiguration,
        const Player &player,
        int score,
        int essence)
    {
        return buildLevelBannerText(levelConfiguration) +
               " | HP " + std::to_string(player.getHealth()) +
               " | Score " + std::to_string(score) +
               " | Essence " + std::to_string(essence) +
               " | N Skip";
    }
}

int main()
{
    sf::RenderWindow window(sf::VideoMode({768, 1024}), "Assail");
    const std::vector<LevelConfiguration> levelConfigurationList = createLevelConfigurationList();
    sf::Music backgroundMusic;
    std::string activeBackgroundMusicTrackId;

    Player player;
    sf::Clock deltaClock;
    DashCooldownIndicator dashIndicator;
    ScreenShakeController screenShakeController;
    std::vector<Enemy> enemies;
    std::vector<Projectile> playerProjectiles;
    std::vector<Projectile> enemyProjectiles;
    std::vector<EnemyDeathEffect> activeEnemyDeathEffects;
    std::vector<FloatingHudRewardText> activeFloatingRewardTexts;
    float enemyFormationDirection = 1.0f;
    float remainingHitPauseSeconds = 0.0f;
    float elapsedSceneTimeSeconds = 0.0f;
    int currentLevelIndex = 0;
    int playerScore = 0;
    int playerEssence = 0;
    std::mt19937 shopRandomNumberGenerator(std::random_device{}());
    LevelProgressState levelProgressState = LevelProgressState::ShowingLevelIntro;
    AppFlowState appFlowState = AppFlowState::TitleScreen;
    TitleMenuSelection titleMenuSelection = TitleMenuSelection::Start;
    RunProgress runProgress;
    RunCodeState runCodeState;

    sf::Font hudFont;
    const bool hasHudFont = loadHudFont(hudFont);

    const auto startFreshRunFromTitle = [&]()
    {
        currentLevelIndex = 0;
        playerScore = 0;
        playerEssence = 0;
        enemyFormationDirection = 1.0f;
        runProgress = {};
        runCodeState.attacksLaunchedThisRun = 0;
        startLevelAttempt(
            levelConfigurationList,
            currentLevelIndex,
            player,
            enemies,
            playerProjectiles,
            enemyProjectiles,
            activeEnemyDeathEffects,
            activeFloatingRewardTexts,
            screenShakeController,
            enemyFormationDirection,
            levelProgressState);
        UpgradeShop::applyPlayerUpgradeStats(player, runProgress.upgrades);
        UpgradeShop::resetLevelStats(runProgress.currentLevelStats);
        levelProgressState = LevelProgressState::ShowingShopIntro;
        appFlowState = AppFlowState::InRun;
        syncBackgroundMusicForLevel(
            levelConfigurationList[currentLevelIndex],
            backgroundMusic,
            activeBackgroundMusicTrackId);
    };

    const auto openShopForCurrentLevel = [&]()
    {
        runProgress.shopState = {};
        runProgress.shopState.currentOffers =
            UpgradeShop::buildShopOffersForLevel(
                levelConfigurationList[currentLevelIndex].levelNumber,
                runProgress.completedLevelStats,
                runProgress.upgrades,
                playerEssence,
                shopRandomNumberGenerator);
        runProgress.shopState.firstPurchaseDiscount =
            runProgress.upgrades.firstPurchaseDiscount;
        runProgress.shopState.statusText =
            "One slot is weighted toward rare or cursed offers. Skipping grants interest.";
        levelProgressState = LevelProgressState::ShowingShop;
    };

    const auto continueFromShopToNextLevel = [&]()
    {
        if (!runProgress.shopState.purchaseMade)
        {
            UpgradeShop::applySkipRewards(
                player,
                playerEssence,
                runProgress.shopState.statusText);
        }

        ++currentLevelIndex;
        startLevelAttempt(
            levelConfigurationList,
            currentLevelIndex,
            player,
            enemies,
            playerProjectiles,
            enemyProjectiles,
            activeEnemyDeathEffects,
            activeFloatingRewardTexts,
            screenShakeController,
            enemyFormationDirection,
            levelProgressState);
        UpgradeShop::applyPlayerUpgradeStats(player, runProgress.upgrades);
        UpgradeShop::resetLevelStats(runProgress.currentLevelStats);
        runProgress.shopState = {};
        levelProgressState = LevelProgressState::ShowingLevelIntro;
        syncBackgroundMusicForLevel(
            levelConfigurationList[currentLevelIndex],
            backgroundMusic,
            activeBackgroundMusicTrackId);
    };

    const auto tryPurchaseShopOfferByIndex = [&](std::size_t offerIndex)
    {
        if (offerIndex >= runProgress.shopState.currentOffers.size())
        {
            return;
        }

        std::string statusMessage;
        UpgradeShop::ShopOffer offerToPurchase =
            runProgress.shopState.currentOffers[offerIndex];
        if (!runProgress.shopState.purchaseMade &&
            runProgress.shopState.firstPurchaseDiscount > 0)
        {
            offerToPurchase.essenceCost =
                std::max(
                    1,
                    offerToPurchase.essenceCost -
                        runProgress.shopState.firstPurchaseDiscount);
        }

        if (UpgradeShop::tryPurchaseShopOffer(
                offerToPurchase,
                player,
                playerEssence,
                runProgress.upgrades,
                statusMessage))
        {
            runProgress.shopState.purchaseMade = true;
            runProgress.shopState.currentOffers.erase(
                runProgress.shopState.currentOffers.begin() +
                static_cast<std::ptrdiff_t>(offerIndex));
        }
        runProgress.shopState.statusText = statusMessage;
    };

    const auto tryRerollShop = [&]()
    {
        if (runProgress.shopState.purchaseMade)
        {
            return;
        }

        std::string statusMessage;
        UpgradeShop::tryRerollShopOffers(
            levelConfigurationList[currentLevelIndex].levelNumber,
            runProgress.completedLevelStats,
            playerEssence,
            runProgress.upgrades,
            runProgress.shopState,
            shopRandomNumberGenerator,
            statusMessage);
        runProgress.shopState.statusText = statusMessage;
    };

    const auto tryTradeInUpgrade = [&]()
    {
        if (runProgress.shopState.purchaseMade || runProgress.shopState.tradeInUsed)
        {
            return;
        }

        std::string statusMessage;
        if (UpgradeShop::tryTradeInLatestUpgrade(
                player,
                playerEssence,
                runProgress.upgrades,
                statusMessage))
        {
            runProgress.shopState.tradeInUsed = true;
        }
        runProgress.shopState.statusText = statusMessage;
    };

    const auto tryDashLeft = [&]()
    {
        const bool couldDashBeforeInput = player.canDash();
        player.dashLeft();
        if (couldDashBeforeInput && player.isPerformingDashAttack())
        {
            UpgradeShop::notifyPlayerDashPerformed(runProgress.upgrades);
        }
    };

    const auto tryDashRight = [&]()
    {
        const bool couldDashBeforeInput = player.canDash();
        player.dashRight();
        if (couldDashBeforeInput && player.isPerformingDashAttack())
        {
            UpgradeShop::notifyPlayerDashPerformed(runProgress.upgrades);
        }
    };

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            if (appFlowState == AppFlowState::CodeEntry)
            {
                if (const auto *textEntered = event->getIf<sf::Event::TextEntered>())
                {
                    if (textEntered->unicode >= 32 && textEntered->unicode <= 126 &&
                        runCodeState.codeEntryBuffer.size() < 24)
                    {
                        runCodeState.codeEntryBuffer.push_back(
                            static_cast<char>(textEntered->unicode));
                    }
                }
            }

            if (const auto *key = event->getIf<sf::Event::KeyPressed>())
            {
                if (appFlowState == AppFlowState::TitleScreen)
                {
                    if (key->code == sf::Keyboard::Key::Escape)
                    {
                        window.close();
                    }
                    if (key->code == sf::Keyboard::Key::Up || key->code == sf::Keyboard::Key::W)
                    {
                        titleMenuSelection = getNextTitleMenuSelection(titleMenuSelection, -1);
                    }
                    if (key->code == sf::Keyboard::Key::Down || key->code == sf::Keyboard::Key::S)
                    {
                        titleMenuSelection = getNextTitleMenuSelection(titleMenuSelection, 1);
                    }
                    if (key->code == sf::Keyboard::Key::Enter ||
                        key->code == sf::Keyboard::Key::Space)
                    {
                        if (titleMenuSelection == TitleMenuSelection::Start)
                        {
                            startFreshRunFromTitle();
                        }
                        else if (titleMenuSelection == TitleMenuSelection::EnterCode)
                        {
                            appFlowState = AppFlowState::CodeEntry;
                            runCodeState.codeEntryStatusMessage.clear();
                        }
                        else
                        {
                            window.close();
                        }
                    }
                    continue;
                }

                if (appFlowState == AppFlowState::CodeEntry)
                {
                    if (key->code == sf::Keyboard::Key::Escape)
                    {
                        appFlowState = AppFlowState::TitleScreen;
                    }
                    else if (key->code == sf::Keyboard::Key::Backspace)
                    {
                        if (!runCodeState.codeEntryBuffer.empty())
                        {
                            runCodeState.codeEntryBuffer.pop_back();
                        }
                    }
                    else if (key->code == sf::Keyboard::Key::Enter)
                    {
                        applyRunCode(runCodeState, runCodeState.codeEntryBuffer);
                        runCodeState.codeEntryBuffer.clear();
                    }
                    continue;
                }

                if (key->code == sf::Keyboard::Key::Escape)
                {
                    window.close();
                }

                if (key->code == sf::Keyboard::Key::N && runCodeState.devModeEnabled)
                {
                    skipToNextLevelForDebugTesting(
                        levelConfigurationList,
                        currentLevelIndex,
                        player,
                        enemies,
                        playerProjectiles,
                        enemyProjectiles,
                        activeEnemyDeathEffects,
                        activeFloatingRewardTexts,
                        screenShakeController,
                        enemyFormationDirection,
                        levelProgressState);
                    UpgradeShop::applyPlayerUpgradeStats(player, runProgress.upgrades);
                    UpgradeShop::resetLevelStats(runProgress.currentLevelStats);
                    levelProgressState = LevelProgressState::ShowingLevelIntro;
                    syncBackgroundMusicForLevel(
                        levelConfigurationList[currentLevelIndex],
                        backgroundMusic,
                        activeBackgroundMusicTrackId);
                    continue;
                }

                if (levelProgressState == LevelProgressState::ShowingLevelStats &&
                    (key->code == sf::Keyboard::Key::Enter ||
                     key->code == sf::Keyboard::Key::Space))
                {
                    openShopForCurrentLevel();
                    continue;
                }

                if (levelProgressState == LevelProgressState::ShowingShop)
                {
                    if (key->code == sf::Keyboard::Key::Num1)
                    {
                        tryPurchaseShopOfferByIndex(0);
                        continue;
                    }
                    if (key->code == sf::Keyboard::Key::Num2)
                    {
                        tryPurchaseShopOfferByIndex(1);
                        continue;
                    }
                    if (key->code == sf::Keyboard::Key::Num3)
                    {
                        tryPurchaseShopOfferByIndex(2);
                        continue;
                    }
                    if (key->code == sf::Keyboard::Key::Num4)
                    {
                        tryPurchaseShopOfferByIndex(3);
                        continue;
                    }
                    if (key->code == sf::Keyboard::Key::R)
                    {
                        tryRerollShop();
                        continue;
                    }
                    if (key->code == sf::Keyboard::Key::T)
                    {
                        tryTradeInUpgrade();
                        continue;
                    }

                    if (key->code == sf::Keyboard::Key::Enter ||
                        key->code == sf::Keyboard::Key::Space)
                    {
                        continueFromShopToNextLevel();
                    }
                    continue;
                }

                if (levelProgressState == LevelProgressState::PlayerDefeated &&
                    (key->code == sf::Keyboard::Key::Enter ||
                     key->code == sf::Keyboard::Key::Space))
                {
                    startLevelAttempt(
                        levelConfigurationList,
                        currentLevelIndex,
                        player,
                        enemies,
                        playerProjectiles,
                        enemyProjectiles,
                        activeEnemyDeathEffects,
                        activeFloatingRewardTexts,
                        screenShakeController,
                        enemyFormationDirection,
                        levelProgressState);
                    if (levelProgressState != LevelProgressState::Victory)
                    {
                        UpgradeShop::applyPlayerUpgradeStats(player, runProgress.upgrades);
                        UpgradeShop::resetLevelStats(runProgress.currentLevelStats);
                        levelProgressState = LevelProgressState::ShowingLevelIntro;
                    }
                    syncBackgroundMusicForLevel(
                        levelConfigurationList[currentLevelIndex],
                        backgroundMusic,
                        activeBackgroundMusicTrackId);
                    continue;
                }

                if (levelProgressState == LevelProgressState::ShowingLevelIntro &&
                    (key->code == sf::Keyboard::Key::Enter ||
                     key->code == sf::Keyboard::Key::Space))
                {
                    levelProgressState = LevelProgressState::Playing;
                    continue;
                }

                if (levelProgressState == LevelProgressState::ShowingShopIntro &&
                    (key->code == sf::Keyboard::Key::Enter ||
                     key->code == sf::Keyboard::Key::Space))
                {
                    levelProgressState = LevelProgressState::ShowingLevelIntro;
                    continue;
                }

                if (levelProgressState != LevelProgressState::Playing)
                {
                    continue;
                }

                const bool dashModifier =
                    key->shift ||
                    key->code == sf::Keyboard::Key::Q ||
                    key->code == sf::Keyboard::Key::E;

                if (key->code == sf::Keyboard::Key::A ||
                    key->code == sf::Keyboard::Key::Left)
                {
                    if (dashModifier)
                    {
                        tryDashLeft();
                    }
                    else
                    {
                        player.stepLeft();
                    }
                }

                if (key->code == sf::Keyboard::Key::D ||
                    key->code == sf::Keyboard::Key::Right)
                {
                    if (dashModifier)
                    {
                        tryDashRight();
                    }
                    else
                    {
                        player.stepRight();
                    }
                }

                if (key->code == sf::Keyboard::Key::Q)
                {
                    tryDashLeft();
                }

                if (key->code == sf::Keyboard::Key::E)
                {
                    tryDashRight();
                }

                if (key->code == sf::Keyboard::Key::Space)
                {
                    player.shoot();
                }
            }

            if (const auto *mouse = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mouse->button == sf::Mouse::Button::Left)
                {
                    const sf::Vector2f mousePosition = {
                        static_cast<float>(mouse->position.x),
                        static_cast<float>(mouse->position.y),
                    };

                    if (appFlowState == AppFlowState::TitleScreen)
                    {
                        if (containsPoint(getTitleButtonBounds(TitleMenuSelection::Start), mousePosition))
                        {
                            titleMenuSelection = TitleMenuSelection::Start;
                            startFreshRunFromTitle();
                        }
                        else if (containsPoint(
                                     getTitleButtonBounds(TitleMenuSelection::EnterCode),
                                     mousePosition))
                        {
                            titleMenuSelection = TitleMenuSelection::EnterCode;
                            appFlowState = AppFlowState::CodeEntry;
                            runCodeState.codeEntryStatusMessage.clear();
                        }
                        else if (containsPoint(getTitleButtonBounds(TitleMenuSelection::Exit), mousePosition))
                        {
                            window.close();
                        }
                        continue;
                    }

                    if (appFlowState == AppFlowState::CodeEntry)
                    {
                        continue;
                    }

                    if (levelProgressState == LevelProgressState::ShowingLevelStats &&
                        containsPoint(getOverlayContinueButtonBounds(), mousePosition))
                    {
                        openShopForCurrentLevel();
                        continue;
                    }

                    if (levelProgressState == LevelProgressState::ShowingShop)
                    {
                        for (std::size_t offerIndex = 0;
                             offerIndex < runProgress.shopState.currentOffers.size();
                             ++offerIndex)
                        {
                            if (containsPoint(UpgradeShop::getCardBounds(offerIndex), mousePosition))
                            {
                                tryPurchaseShopOfferByIndex(offerIndex);
                                continue;
                            }
                        }

                        if (containsPoint(UpgradeShop::getRerollButtonBounds(), mousePosition))
                        {
                            tryRerollShop();
                            continue;
                        }

                        if (containsPoint(UpgradeShop::getTradeInButtonBounds(), mousePosition))
                        {
                            tryTradeInUpgrade();
                            continue;
                        }

                        if (containsPoint(getOverlayContinueButtonBounds(), mousePosition))
                        {
                            continueFromShopToNextLevel();
                            continue;
                        }
                    }

                    if (levelProgressState == LevelProgressState::ShowingLevelIntro &&
                        containsPoint(getOverlayContinueButtonBounds(), mousePosition))
                    {
                        levelProgressState = LevelProgressState::Playing;
                        continue;
                    }

                    if (levelProgressState == LevelProgressState::ShowingShopIntro &&
                        containsPoint(getOverlayContinueButtonBounds(), mousePosition))
                    {
                        levelProgressState = LevelProgressState::ShowingLevelIntro;
                        continue;
                    }

                    if (levelProgressState == LevelProgressState::PlayerDefeated &&
                        containsPoint(getOverlayContinueButtonBounds(), mousePosition))
                    {
                        startLevelAttempt(
                            levelConfigurationList,
                            currentLevelIndex,
                            player,
                            enemies,
                            playerProjectiles,
                            enemyProjectiles,
                            activeEnemyDeathEffects,
                            activeFloatingRewardTexts,
                        screenShakeController,
                        enemyFormationDirection,
                        levelProgressState);
                    UpgradeShop::applyPlayerUpgradeStats(player, runProgress.upgrades);
                    UpgradeShop::resetLevelStats(runProgress.currentLevelStats);
                    levelProgressState = LevelProgressState::ShowingLevelIntro;
                    syncBackgroundMusicForLevel(
                            levelConfigurationList[currentLevelIndex],
                            backgroundMusic,
                            activeBackgroundMusicTrackId);
                        continue;
                    }
                }

                if (levelProgressState == LevelProgressState::Playing &&
                    mouse->button == sf::Mouse::Button::Right)
                {
                    player.shoot();
                }
            }
        }

        const float deltaTime = deltaClock.restart().asSeconds();
        elapsedSceneTimeSeconds += deltaTime;
        if (appFlowState == AppFlowState::InRun)
        {
            player.updateVisualFeedback(deltaTime);
            updateEnemyIdleAnimation(enemies, deltaTime);
            updateEnemyDeathEffects(activeEnemyDeathEffects, deltaTime);
            updateFloatingRewardTexts(activeFloatingRewardTexts, deltaTime);
        }

        if (appFlowState == AppFlowState::InRun && remainingHitPauseSeconds > 0.0f)
        {
            remainingHitPauseSeconds = std::max(0.0f, remainingHitPauseSeconds - deltaTime);
        }
        else if (appFlowState == AppFlowState::InRun &&
                 levelProgressState == LevelProgressState::Playing)
        {
            const LevelConfiguration &currentLevelConfiguration =
                levelConfigurationList[currentLevelIndex];
            FrameCombatFeedback frameCombatFeedback;
            const float difficultyScale =
                getDifficultyScale(currentLevelIndex, playerScore, runProgress);
            updateLevelStatsTimers(runProgress.currentLevelStats, deltaTime);

            player.handleInput(deltaTime);
            if (const std::optional<float> launchDelaySeconds =
                    player.consumePendingProjectileLaunchDelay())
            {
                const UpgradeShop::FiredShotPlan shotPlan =
                    UpgradeShop::consumeNextShotPlan(
                        runProgress.currentLevelStats,
                        runProgress.upgrades);
                firePlayerProjectiles(
                    player,
                    playerProjectiles,
                    *launchDelaySeconds,
                    shotPlan,
                    runCodeState);
                ++runProgress.currentLevelStats.shotsFired;
            }

            updateMovingFormationEnemies(
                currentLevelConfiguration,
                enemies,
                deltaTime,
                difficultyScale,
                static_cast<float>(window.getSize().x),
                enemyFormationDirection);

            if (aliveEnemyReachedBottomOfScreen(
                    enemies,
                    static_cast<float>(window.getSize().y)))
            {
                const int healthBeforeDefeat = player.getHealth();
                player.setHealth(0);
                frameCombatFeedback.playerTookDamageThisFrame = true;
                frameCombatFeedback.playerDamageTakenThisFrame += healthBeforeDefeat;
            }

            letProjectileEnemiesFireProjectiles(
                currentLevelConfiguration,
                enemies,
                player,
                enemyProjectiles,
                difficultyScale,
                deltaTime,
                frameCombatFeedback);

            for (Projectile &playerProjectile : playerProjectiles)
            {
                playerProjectile.update(deltaTime);
            }

            for (Projectile &enemyProjectile : enemyProjectiles)
            {
                enemyProjectile.update(deltaTime);
            }

            damageEnemiesHitByPlayerProjectiles(
                playerProjectiles,
                enemies,
                player,
                frameCombatFeedback,
                runProgress.upgrades,
                hasHudFont ? &hudFont : nullptr);
            damageEnemiesHitByPlayerDash(
                player,
                enemies,
                frameCombatFeedback,
                hasHudFont ? &hudFont : nullptr);
            collectDashNearMissRewards(
                enemyProjectiles,
                player,
                frameCombatFeedback,
                hasHudFont ? &hudFont : nullptr);
            damagePlayerHitByEnemyProjectiles(
                enemyProjectiles,
                player,
                frameCombatFeedback);

            if (frameCombatFeedback.playerTookDamageThisFrame && player.isAlive())
            {
                if (runProgress.upgrades.spiteButtonBonusDamage > 0)
                {
                    runProgress.upgrades.pendingNextShotBonusDamage +=
                        runProgress.upgrades.spiteButtonBonusDamage;
                }
                damageEnemiesNearPoint(
                    enemies,
                    player.getPosition(),
                    2000.0f,
                    runProgress.upgrades.spitefulCoreDamage,
                    frameCombatFeedback,
                    hasHudFont ? &hudFont : nullptr,
                    nullptr,
                    "Spite",
                    sf::Color(255, 120, 160));
            }
            UpgradeShop::applyPlayerUpgradeStats(player, runProgress.upgrades);

            // The frame feedback object batches all rewards first, then the totals are
            // applied once here so the loop has a single source of truth.
            playerScore += frameCombatFeedback.scoreEarnedThisFrame;
            playerEssence += frameCombatFeedback.essenceEarnedThisFrame;
            runProgress.currentLevelStats.shotsHit += frameCombatFeedback.playerShotsHitThisFrame;
            runProgress.currentLevelStats.dashKills += frameCombatFeedback.dashKillsThisFrame;
            runProgress.currentLevelStats.nearMisses += frameCombatFeedback.nearMissesThisFrame;
            runProgress.currentLevelStats.damageTaken += frameCombatFeedback.playerDamageTakenThisFrame;
            runProgress.currentLevelStats.scoreEarned += frameCombatFeedback.scoreEarnedThisFrame;
            runProgress.currentLevelStats.essenceEarned += frameCombatFeedback.essenceEarnedThisFrame;
            runProgress.currentLevelStats.enemiesDefeated += frameCombatFeedback.enemiesDefeatedThisFrame;
            if (frameCombatFeedback.playerTookDamageThisFrame)
            {
                runProgress.currentLevelStats.tookDamage = true;
                runProgress.currentLevelStats.currentCombo = 0;
                runProgress.currentLevelStats.comboGraceSecondsRemaining = 0.0f;
            }
            for (int enemyDefeatIndex = 0;
                 enemyDefeatIndex < frameCombatFeedback.enemiesDefeatedThisFrame;
                 ++enemyDefeatIndex)
            {
                registerEnemyDefeatForCombo(runProgress.currentLevelStats);
            }
            removeDefeatedEnemies(enemies);
            removeInactiveProjectiles(playerProjectiles);
            removeInactiveProjectiles(enemyProjectiles);
            appendEnemyDeathEffects(
                activeEnemyDeathEffects,
                frameCombatFeedback.enemyDeathEffectsToSpawn);
            appendFloatingRewardTexts(
                activeFloatingRewardTexts,
                frameCombatFeedback.floatingRewardTextsToSpawn);

            if (!frameCombatFeedback.enemyDeathEffectsToSpawn.empty())
            {
                startHitPause(remainingHitPauseSeconds, ShortHitPauseDurationSeconds);
                screenShakeController.addShake(0.10f, 3.2f);
            }

            if (frameCombatFeedback.dashHitEnemyThisFrame)
            {
                screenShakeController.addShake(0.10f, 3.8f);
            }

            if (frameCombatFeedback.bossTookDamageThisFrame)
            {
                startHitPause(remainingHitPauseSeconds, LargeHitPauseDurationSeconds);
                screenShakeController.addShake(0.13f, 4.2f);
            }

            if (frameCombatFeedback.playerTookDamageThisFrame)
            {
                startHitPause(remainingHitPauseSeconds, ShortHitPauseDurationSeconds);
                screenShakeController.addShake(0.15f, 4.8f);
            }

            if (frameCombatFeedback.bossAttackTriggeredThisFrame)
            {
                screenShakeController.addShake(0.08f, 2.4f);
            }

            if (!player.isAlive())
            {
                levelProgressState = LevelProgressState::PlayerDefeated;
            }
            else if (enemies.empty())
            {
                const bool isLastLevel =
                    currentLevelIndex >= static_cast<int>(levelConfigurationList.size()) - 1;
                awardLevelClearBonuses(
                    runProgress.currentLevelStats,
                    playerScore,
                    playerEssence);
                runProgress.completedLevelStats = runProgress.currentLevelStats;
                levelProgressState =
                    isLastLevel ? LevelProgressState::Victory
                                : LevelProgressState::ShowingLevelStats;
                playerProjectiles.clear();
                enemyProjectiles.clear();
            }
        }

        if (appFlowState == AppFlowState::InRun)
        {
            screenShakeController.update(deltaTime);
        }

        window.clear();

        if (appFlowState == AppFlowState::TitleScreen && hasHudFont)
        {
            window.setView(window.getDefaultView());
            drawTitleScreen(window, hudFont, titleMenuSelection, runCodeState);
        }
        else if (appFlowState == AppFlowState::CodeEntry && hasHudFont)
        {
            window.setView(window.getDefaultView());
            drawCodeEntryScreen(window, hudFont, runCodeState);
        }
        else
        {
            sf::View gameplayView = window.getDefaultView();
            gameplayView.move(screenShakeController.getCurrentOffset(elapsedSceneTimeSeconds));
            window.setView(gameplayView);

            dashIndicator.setCooldownProgress(player.getDashCooldownProgress());
            dashIndicator.setDashEmphasis(player.getDashVisualStrength());
            dashIndicator.setPosition({384.0f, 770.0f});

            if (hasHudFont)
            {
                const LevelConfiguration &currentLevelConfiguration =
                    levelConfigurationList[currentLevelIndex];
                drawTopHudBoxes(
                    window,
                    hudFont,
                    currentLevelConfiguration,
                    player,
                    playerScore,
                    playerEssence,
                    runProgress.currentLevelStats,
                    runCodeState);

                if (const Enemy *bossEnemy = findPrimaryBoss(enemies))
                {
                    drawBossHealthBar(window, hudFont, *bossEnemy, currentLevelConfiguration);
                }
            }

            window.draw(dashIndicator);

            for (const Enemy &enemy : enemies)
            {
                window.draw(enemy);
            }

            for (const EnemyDeathEffect &enemyDeathEffect : activeEnemyDeathEffects)
            {
                window.draw(enemyDeathEffect);
            }

            for (const FloatingHudRewardText &floatingRewardText : activeFloatingRewardTexts)
            {
                window.draw(floatingRewardText.text);
            }

            for (const Projectile &playerProjectile : playerProjectiles)
            {
                window.draw(playerProjectile);
            }

            for (const Projectile &enemyProjectile : enemyProjectiles)
            {
                window.draw(enemyProjectile);
            }

            window.draw(player);

            if (hasHudFont && levelProgressState != LevelProgressState::Playing)
            {
                window.setView(window.getDefaultView());
                drawOverlayPanel(
                    window,
                    hudFont,
                    levelConfigurationList,
                    currentLevelIndex,
                    levelProgressState,
                    runProgress,
                    playerEssence);
            }

            window.setView(window.getDefaultView());
        }

        window.display();
    }

    return 0;
}
