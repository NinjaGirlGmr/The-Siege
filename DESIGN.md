# Design Document — The Siege

*A vertical shooter roguelite built in C++ with SFML 3*
*Created for AP Computer Science Principles — April 2026*

---

## Overview

The Siege is a lane-based shooter where players fight through waves of enemies, earn essence, and spend it in an upgrade shop between levels. I built it over roughly two weeks as my AP CSP exam project. Beyond fulfilling the exam requirements, it became a genuine creative project — I wrote the code, created the art, and composed the music.

---

## Why C++

I had built a 2D game before in Java, so I already had a reference point going in. Before starting this project I read about how both Java and C++ handle game development at a low level.

Java runs on the JVM — your source compiles to bytecode, which is then interpreted or JIT-compiled at runtime. That abstraction is convenient, but it introduces overhead and reduces how much control you have over what the program actually does moment-to-moment. C++ compiles directly to machine code. There is no runtime layer between the program and the hardware.

For a game, that distinction matters. I wanted frame-accurate timing, precise control over memory, and the ability to make deliberate decisions about how every system runs. C++ gave me that. It also helped that I had been actively learning C++ and wanted a real project to apply it to. AAA games are largely built in C++, so I knew the architecture would scale to something meaningful.

---

## How It Meets the AP CSP Requirements

### 1. User Input

The game accepts input in two forms:

- **Real-time keyboard input** — movement between lanes, dashing, and spell casting during gameplay
- **Code entry** — a title screen that accepts typed codes to unlock special modes (inspired by the Konami code concept)

### 2. Feedback Loop

The core feedback loop is the upgrade shop. After every level, the game measures how you played — your accuracy, how many enemies you dashed through, how many near-misses you landed, and how much damage you took. Those stats feed directly into which upgrades appear in the shop next.

If you're dashing aggressively, the shop offers upgrades that reward dashing. If you're playing it safe, it leans toward defensive options. The shop adapts to you. That creates a loop where your playstyle shapes the upgrades you're offered, and your upgrades reinforce or redirect your playstyle going forward.

The struct that drives this is `LevelStats`, which records performance across every level:

```cpp
struct LevelStats {
    int shotsFired, shotsHit;
    int dashKills, nearMisses;
    int damageTaken, scoreEarned;
    int enemiesDefeated, maxCombo;
    // ...
};
```

That data is passed into `buildShopOffersForLevel()`, which weights the available upgrade pool based on what the player actually did.

### 3. Output

The game outputs continuous visual feedback: the arena state, health, score, combo counter, cooldown indicators, screen shake on impact, and particle effects on enemy deaths. At the end of each level it shows a stats screen, and at the end of the run it shows either a victory screen or a defeat screen.

---

## Building the Animation System

The hardest technical problem I solved was the sprite animation system.

I knew I wanted animated characters, which meant sprite sheets — a single image containing every frame of an animation laid out in a grid. The first challenge was that different entities had different grid sizes, different frame counts, and different playback speeds. A basic enemy plays 8 frames in a loop. The ranged enemy has 27 frames spread across an attack cycle. The player switches between an idle sheet and a separate cast sheet, and the timing of the projectile spawn has to be synchronized to a specific frame of the cast animation.

I solved this by tying animation states to named strings. Each entity registers its animations by name — `"rest"`, `"spellBasic"`, `"shieldIdle"`, etc. — and stores the corresponding list of texture rectangles and the correct texture reference. Switching animations is then just a matter of calling `setAnimationState("rest")`, and the system handles swapping the texture and rewinding to frame zero. Playback runs off a per-entity time accumulator, so frame rate is independent of render rate.

The scaling issue came from the sprites being authored at 64×64 pixels, which is small on a modern display. Every entity has a configurable `spriteSize_` that the system uses to compute a scale factor before rendering. That way sprites can be displayed at any target size without changing the source art.

---

## The Shop: How It Evolved

There was no shop in early development.

The first version of upgrades was random: complete a level, get a random stat boost. It was chaotic — values would stack past sensible limits, or accidentally modify the wrong state, or reset unexpectedly. I spent a while trying to fix the edge cases and eventually just removed upgrades entirely. It felt like more trouble than it was worth.

Then I played through the whole game without them.

It was fine mechanically, but it felt empty. Fighting enemies is the game, but without any sense of progression between levels there was nothing to look forward to. Completing a level felt like nothing. I came back to the upgrade idea and this time designed it properly — a shop with explicit, named offers and a clear transaction model. No hidden state, no random triggers.

Once that foundation was in place I started adding upgrades one at a time and testing each one. Some created new ways to play: `PiercingRune` lets shots pass through enemies, `EchoWeave` fires a follow-up projectile on a cadence, `VampireBolt` heals you on kills. Some are straightforward damage or defense boosts. Some — `EmotionalSupportRock`, `TaxEvasion`, `QuestionableSmoothie` — are jokes. They do something, but the fun is partly in the surprise.

The game went from feeling repetitive to feeling like each run was its own thing.

---

## Beta Testing

I ran informal playtests with a diverse group of people — friends who play games constantly and friends who almost never do. I told some of them how to play and let others figure it out on their own.

A few things came out of that:

**Enemy variety.** Testers noticed that all enemies looked the same early on. The different behavior profiles (Basic, Shielded, Shooter, Agile) were already in the code, but visually they blurred together. I differentiated them with distinct sprites, color shifts, and animation styles so players could read the threat type at a glance.

**Upgrade volume and tone.** The original shop had around five items. Testers found it repetitive quickly. The suggestion to mix serious upgrades with silly ones came from this feedback. Once I added items like the Emotional Support Rock and Tax Evasion, players started looking forward to opening the shop just to see what came up. The humor became part of the reward.

**The codes.** I originally added codes as a development tool — typing `DEVMODE` let me skip to any level. The `EMOTIONALDAMAGE` code was a joke for friends. After testers discovered them and responded positively, I treated the code system as a real feature. Several people independently suggested I make a Konami-style code system, which is something I plan to add. I also want to remove the list of available codes from the UI and hide them as in-game secrets instead — something players discover rather than read.

---

## Art and Music

I am not a pixel artist. My background is in drawing and 3D modeling. So rather than try to learn pixel art from scratch, I found a workaround using tools I already knew.

For each character, I built a low-poly 3D model in Blender, textured and rigged it, and animated it. Then I used Blender's render pipeline to output each frame at 64×64 pixels with a transparent background. By skipping every few frames from the 3D animation, I got a sprite sheet that reads as pixel art at game resolution. The whole pipeline — modeling, rigging, animating, and rendering the sprites — took about three hours per character on the first attempt.

The music I wrote myself. I have played guitar for almost a decade but have no formal training in music theory. Writing and producing game audio was new to me. The current track (`TheSiegeSong1`) is the second piece of music I've ever written. I'd like to write more before the project is complete.

---

## What I'd Do Differently

- The game loop in `main.cpp` grew very large. If I started over I would break the level management, input handling, and rendering into separate modules earlier, rather than waiting until they got unwieldy.
- The random upgrade system I abandoned early on had the right idea but wrong execution. A better version of it could live alongside the shop as a modifier layer — random passive effects that stack on top of deliberate purchases.
- I would formalize the beta testing earlier. The feedback I got shaped some of the best parts of the game, and I would have gotten more of it if I had started asking sooner.

---

## What's Next

- Konami-style secret code sequences discoverable through in-game clues
- A "kid mode" with cartoon or meme-based art, suggested by testers
- Additional music tracks
- More upgrade items and code-unlockable modifiers
- Removing the visible code list and making the codes discoverable in-world
