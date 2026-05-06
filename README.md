# 🎯 3D Shooting Arena

> A first-person shooter game built from scratch in **C++** using **OpenGL/GLUT** for graphics and **OpenAL** for 3D spatial audio — no game engine, no shaders, no external assets.

---

## 👤 Author

| Field | Info |
|-------|------|
| **Name** | Muhammed Awad Farag Hamed |
| **Course** | Computer Graphics |

---

## 📖 About

3D Shooting Arena is a fully playable FPS game where the player moves freely inside a walled arena, aims at enemy targets, and scores points before a **60-second timer** runs out. Every system — physics, audio, particles, AI, and the HUD — is hand-built from scratch in a single C++ source file.

---
⚙️ Video For Showing a live game
https://drive.google.com/file/d/13PZnZ9uZM4wENFb7zZOywakXN24uopbL/view?usp=drive_link
## ⚙️ Tech Stack

| Layer | Technology |
|-------|-----------|
| Language | C++17 |
| Graphics | OpenGL (fixed-function) + GLUT / freeglut |
| Audio | OpenAL 1.1 + libsndfile |
| Math | Custom `Vec3` struct (no GLM) |
| Window | 1280 × 720 |
| Target FPS | ~60 (16 ms timer loop) |

---

## 🕹️ Controls

| Key / Input | Action |
|-------------|--------|
| `W A S D` | Move |
| `Mouse` | Aim / Look around |
| `Left Click` | Shoot (hold to auto-fire) |
| `R` | Reload |
| `Space` | Jump |
| `Shift` | Sprint |
| `Arrow Keys` | Rotate camera |
| `P` | Pause / Resume |
| `ESC` | Quit |
| `R` *(game over)* | Restart |

---

## 🏗️ Build & Run

### Dependencies
```bash
# Ubuntu / Debian
sudo apt install freeglut3-dev libopenal-dev libsndfile1-dev

# macOS
brew install freeglut openal-soft libsndfile
```

### Compile
```bash
g++ shooting_game.cpp -o game -lGL -lGLU -lglut -lm -lopenal -lsndfile
```

### Run
```bash
./game
```

> ⚠️ Place your background music file at `assets/Kiwi.mp3` before running.  
> If no audio device is found, the game runs silently — no crash.

---

## 🎮 Gameplay

- **60 seconds** on the clock — score as many points as possible.
- Destroy targets to earn points. Combos (3+ kills in a row) give **bonus points**.
- Targets **respawn** at random positions after a few seconds.
- Difficulty **scales over time** — respawn delays get shorter.
- Watch your **health** — it drops if the vignette flares red on screen edges.

### Target Types

| Type | Behavior | HP | Points |
|------|----------|----|--------|
| 🔴 Static | Stands still, easy to hit | 1 | 50 |
| 🟢 Patrol | Moves back and forth between two points | 2 | 100 |
| 🔵 Rotate | Spins in place | 2 | 120 |
| 🟠 Bouncer | Bounces off arena walls like a billiard ball | 3 | 150 |
| 🚁 Drone | Flies with sinusoidal motion *(disabled by default)* | 3 | 200 |

---

## 🔊 Audio System

All 8 sound effects are **synthesized at startup** from math — no WAV files needed:

| Sound | How it's made |
|-------|--------------|
| Gunshot | Noise burst + 180 Hz crack + 1200 Hz click |
| Reload | 3-phase: eject click → slide scrape → chamber snap |
| Footstep | Low-freq noise + 60 Hz thud |
| Jump | Swept sine 120 → 940 Hz |
| Land | Heavy thud scaled by fall speed |
| Explosion | 35+55 Hz rumble + noise burst |
| Hit | 740 Hz metallic ping + harmonic |
| Empty click | 1600 Hz dry tap |

Hit and explosion sounds are **3D positional** — they get louder as you get closer to the target.

---

## ✨ Features

- **First-person camera** with free mouse look, head bobbing, jump, and recoil
- **Ray-sphere hit detection** — precise bullet raycast against every target's collision sphere
- **Particle system** — 600-particle pool for muzzle flash, bullet impacts, and explosions
- **Weapon view model** — animated gun in the corner with recoil and bob
- **Full HUD** — health bar, ammo bar, score, timer, combo counter, accuracy, FPS
- **3D spatial audio** — 16-source polyphonic pool, listener follows the camera
- **Procedural skybox** — gradient sky, 200 stars, orbiting moon
- **Fog** — linear depth fog for atmosphere
- **Damage vignette** — red screen edges when health drops below 40
- **Sci-fi decorations** — crate clusters, glowing platform, orbiting ring, runway lines

---

## 📐 Arena Layout

```
+--------------------------------------------------+
|                                                  |
|   [Crates]        [Drone zone]      [Crates]     |
|                                                  |
|      (Patrol)          (Static)                  |
|                                                  |
|  (Bouncer)      ★ PLAYER START ★     (Rotate)   |
|                                                  |
|         [Sci-fi Platform]    (Static)            |
|                                                  |
|   (Bouncer)         (Patrol)      [Crates]       |
|                                                  |
+--------------------------------------------------+
        Arena: 100 × 100 units
```

---

## 📏 Key Constants

| Constant | Value | Meaning |
|----------|-------|---------|
| `ARENA_SIZE` | 50 | Half-extent of arena (total 100 units wide) |
| `PLAYER_SPEED` | 7.0 | Normal movement speed |
| `SPRINT_MULT` | 1.9× | Sprint multiplier |
| `JUMP_VEL` | 7.5 | Jump launch velocity |
| `GRAVITY` | −18 | Gravity strength |
| `MAX_AMMO` | 30 | Rounds per magazine |
| `RELOAD_TIME` | 1.8 s | Time to reload |
| `SHOOT_COOLDOWN` | 0.12 s | Minimum time between shots |
| `MOUSE_SENS` | 0.12 | Mouse sensitivity |
| `MAX_PARTICLES` | 600 | Particle pool size |

---

## 🗂️ Code Structure

```
shooting_game.cpp
│
├── Sound System        → soundInit, playSound, buildShoot, buildExplosion ...
├── Camera System       → struct Camera, initCamera, updatePlayer
├── Target System       → struct Target, initTargets, updateTargets
├── Particle System     → struct Particle, spawnImpact, spawnExplosion, renderParticles
├── Game State          → struct GameState, initGame
├── Input Handling      → onKey, onMouseMove, onMouseClick
├── Shooting            → doShoot, raySphere
├── Rendering           → display, drawGround, drawWalls, drawTarget, drawSkybox
├── HUD                 → drawHUD, drawHealthBar, drawAmmoBar, drawCrosshair
└── Main Loop           → main, update, reshape
```

---

## 📋 Function Reference

| Function | What it does |
|----------|-------------|
| `soundInit()` | Sets up OpenAL, creates audio buffers and sources |
| `soundShutdown()` | Frees all OpenAL resources on exit |
| `playSound(id, ...)` | Plays a sound by ID with volume, pitch, and optional 3D position |
| `soundUpdateListener(...)` | Moves the OpenAL listener to match the camera |
| `soundUpdateMovement(...)` | Triggers footstep, jump, and land sounds |
| `musicInit(filepath)` | Loads and loops a background music file |
| `musicShutdown()` | Stops and frees the music source |
| `musicSetVolume(vol)` | Sets music volume (0.0 – 1.0) |
| `buildShoot()` | Synthesizes gunshot PCM samples |
| `buildReload()` | Synthesizes reload PCM samples |
| `buildFootstep()` | Synthesizes footstep PCM samples |
| `buildJump()` | Synthesizes jump whoosh PCM samples |
| `buildLand()` | Synthesizes landing thud PCM samples |
| `buildExplosion()` | Synthesizes explosion PCM samples |
| `buildHit()` | Synthesizes metallic hit ping PCM samples |
| `buildEmpty()` | Synthesizes dry-fire click PCM samples |
| `uploadBuffer(...)` | Uploads a PCM vector into an OpenAL buffer |
| `getFreeSource()` | Finds a free audio source from the 16-slot pool |
| `initParticles()` | Resets all particle slots to inactive |
| `allocParticle()` | Returns the next free particle slot |
| `spawnImpact(pos, ...)` | Spawns 18 spark particles at a hit point |
| `spawnExplosion(pos)` | Spawns 60 fire particles for an explosion |
| `spawnMuzzleFlash(pos)` | Spawns 8 bright particles at the gun barrel |
| `updateParticles(dt)` | Moves particles, applies gravity, fades and removes them |
| `initCamera()` | Resets camera to start position and clears all state |
| `initTargets()` | Places all targets in the arena |
| `addTarget(...)` | Adds one target with given type, position, HP, and score |
| `updateTargets(dt, time)` | Runs AI movement for every target each frame |
| `initGame()` | Full game reset — score, health, ammo, camera, targets, particles |
| `doShoot()` | Fires a bullet, raycasts for hits, triggers effects and sounds |
| `updatePlayer(dt)` | Moves player, runs physics, updates head bob and recoil |
| `raySphere(...)` | Returns ray-sphere intersection distance or −1 if no hit |
| `drawBox(hw, hh, hd)` | Draws a solid box at the current matrix origin |
| `drawGround()` | Renders the checkerboard floor with grid lines |
| `drawWalls()` | Renders four arena walls with neon top trim |
| `drawPillar(x, z, h, r)` | Draws a cylindrical pillar with glowing cap |
| `drawCrate(...)` | Draws a single wooden crate |
| `drawCrateFull(...)` | Draws a crate with wireframe overlay |
| `drawShadow(x, z, r)` | Draws a fake circular shadow on the ground |
| `drawTarget(t, time)` | Renders one target with shape, HP bar, and shadow |
| `drawSkybox()` | Renders gradient sky, stars, and orbiting moon |
| `drawDecorations(time)` | Renders crates, platform, decorative ring, runway lines |
| `renderParticles()` | Draws all active particles as camera-facing billboards |
| `drawWeaponViewModel()` | Renders the animated gun in the corner of the screen |
| `drawHUD()` | Draws all 2D overlays: bars, score, timer, crosshair, FPS |
| `drawGameOverScreen()` | Shows the game-over overlay with final stats |
| `drawPauseScreen()` | Shows the PAUSED overlay |
| `drawCrosshair()` | Draws the dynamic spread crosshair |
| `drawAmmoBar()` | Draws the ammo bar and reload status |
| `drawHealthBar()` | Draws the health bar (green → red) |
| `drawText2D(...)` | Renders text at a screen position |
| `drawRect2D(...)` | Draws a filled 2D rectangle (bars, overlays) |
| `setupLighting()` | Configures sun light, blue accent, and red accent lights |
| `display()` | Main render function — draws the full scene every frame |
| `update(int)` | Main tick — physics, AI, audio, timers, game logic |
| `reshape(w, h)` | Adjusts the OpenGL viewport on window resize |
| `onKey(...)` | Key press handler |
| `onKeyUp(...)` | Key release handler |
| `onSpecialKey(...)` | Arrow key press handler |
| `onSpecialKeyUp(...)` | Arrow key release handler |
| `onMouseClick(...)` | Mouse button handler — left click shoots |
| `onMouseMove(...)` | Mouse move handler — updates camera aim |

---

*Built with ❤️ using raw OpenGL — no engine, no shortcuts.*