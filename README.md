# Assail — Game for AP Class

Assail- a form of attack.

A vertical shooter roguelite built in C++ with SFML 3.

## Overview

The player fights through waves of enemies in a lane-based arena, earning essence to spend in an upgrade shop between levels. Each run is unique thanks to a procedurally weighted shop system with 30+ upgrades across four rarity tiers.

## How to Play

- **Arrow keys / A / D** — Move left or right between lanes
- **Shift** — Dash (brief invulnerability + dash attack, cooldown applies)
- **Hold shoot key** — Fire spells continuously
- Defeat all enemies in a wave to advance to the next level
- Spend essence in the shop between levels to upgrade your build

## Enemy Types

| Type | Description |
|---|---|
| Basic Grunt | Standard enemy in formation |
| Shielded | Blocks attacks until shield is broken |
| Shooter | Fires projectiles back at the player |
| Agile | Faster movement, harder to hit |
| Mini Boss | High health, high essence reward |
| Final Boss | End-of-run encounter |

## Upgrade Rarity Tiers

- **Common** — Reliable stat boosts
- **Rare** — Situational power spikes
- **Legendary** — Run-defining effects
- **Cursed** — High risk / high reward

## Building

Requires CMake 3.16+ and SFML 3 (via Homebrew on macOS).

```bash
cmake -S . -B build
cmake --build build
./build/game
```

## Project Structure

```
src/
  main.cpp                    — Game loop, level flow, input, rendering
  UpgradeShop.cpp/hpp         — Shop economy, upgrade logic, 30+ upgrades
  entities/
    Entity.cpp/hpp            — Base class (health, defense, sprite)
    Player.cpp/hpp            — Lane movement, dash, spell animation
    Enemy.cpp/hpp             — Enemy types, behavior profiles, AI
    Projectile.cpp/hpp        — Projectile physics and collision
  UI/
    DashCooldownIndicator     — Circular dash charge display
    EnemyDeathEffect          — Particle burst on enemy defeat
    ScreenShakeController     — Trauma-based screen shake
resources/
  sprites/                    — PNG sprite sheets
  audio/                      — OGG music tracks
```

## Run Codes

Access from the title screen to enable special modes:
- `DEVMODE` — Developer mode
- `EMOTIONALDAMAGE` — Optional challenge modifier

---

## Documentation

- [DESIGN.md](DESIGN.md) — Full design document covering language choice, technical challenges, how the shop evolved, beta testing, and art/music process

## Update Timeline

| Date | Update |
|---|---|
| 2026-04-16 | Added background music (AssailSong1) |
| 2026-04-22 | Initial README and DESIGN.md created; project includes enemy behavior profiles, 30+ upgrades, screen shake, dash cooldown UI, and death particle effects |
