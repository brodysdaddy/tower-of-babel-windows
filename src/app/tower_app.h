// tower_app.h - screens, visuals and sound for Tower of Babel.
//
// Portable presentation layer: it turns button presses into game actions and
// draws everything into a Framebuffer. A platform just calls update() and
// render() every frame, then shows the buffer.
//
// Two buttons:
//   front - start / drop a block / play again
//   side  - pause / resume / back to the title screen
#pragma once
#include <stdint.h>
#include "../core/tower_game.h"
#include "../gfx/framebuffer.h"
#include "platform.h"

class TowerApp {
public:
    // frontName / sideName are what the hints call the buttons, e.g.
    // "SPACE" and "P" on Windows, "FRONT" and "SIDE" on the stick.
    // hasExit: the side button on the title screen asks to leave the game
    // (the stick goes back to its game selector). Windows has no use for it.
    TowerApp(uint32_t seed, const char* frontName, const char* sideName, bool hasExit = false);

    void update(float dt, bool frontPressed, bool sidePressed);
    void render(Framebuffer& fb);
    void pause();   // e.g. when the window loses focus
    void enterTitle();                                   // fresh title screen
    bool exitRequested() const { return exitRequested_; }
    const TowerGame& game() const { return game_; }      // read-only, for diagnostics

private:
    enum class Mode : uint8_t { Title, Playing, Paused, GameOver };

    struct Debris   { bool active; float cx, cy, w, h, vx, vy, angle, spin; Rgb color; };
    struct Particle { float x, y, vx, vy, life, maxLife, gravity; Rgb color; uint8_t size; };
    struct Cloud    { float x, sy0, speed, r; };
    struct Star     { uint8_t x, y, big; float phase; };

    static constexpr int MAX_DEBRIS = 6, MAX_PARTICLES = 64, NUM_CLOUDS = 5, NUM_STARS = 48;

    void startGame();
    void handleDrop();
    void gameOver();
    void spawnDebris(const Slab& s, int layerIndex, float dir);
    void spawnParticle(float x, float y, float vx, float vy, float life, float gravity,
                       Rgb color, uint8_t size);
    void updateEffects(float dt);
    void play(const Tone* notes, uint8_t count);
    void playPerfect(int combo);

    float rand01();
    float randRange(float lo, float hi) { return lo + (hi - lo) * rand01(); }
    float screenY(float worldY) const;          // world height -> screen row
    float layerTopY(int i) const;               // screen row of a layer's top
    static Rgb layerColor(int i);

    void drawBackground(Framebuffer& fb);
    void drawSlab(Framebuffer& fb, int i, const Slab& s, float topY, float flash);
    void drawEffects(Framebuffer& fb);
    void drawHud(Framebuffer& fb);
    void drawOverlay(Framebuffer& fb);
    void drawCentered(Framebuffer& fb, int y, const char* s, Rgb c, int scale, int alpha = 255);

    TowerGame game_;
    Mode      mode_ = Mode::Title;
    uint32_t  rng_;
    float     time_ = 0, modeTime_ = 0;
    float     cam_ = 0;            // how far the view has scrolled up (px)
    float     shake_ = 0, shakeY_ = 0;
    float     flash_ = 0;          // white flash on a perfect landing
    uint32_t  best_ = 0;
    bool      newBest_ = false;

    Debris    debris_[MAX_DEBRIS];
    Particle  parts_[MAX_PARTICLES];
    Cloud     clouds_[NUM_CLOUDS];
    Star      stars_[NUM_STARS];
    Rgb       skyRow_[Framebuffer::H];

    char      popup_[16] = "";
    char      popup2_[16] = "";
    float     popupLife_ = 0, popupY_ = 0;

    char      frontName_[12];
    char      sideName_[12];
    bool      hasExit_ = false;
    bool      exitRequested_ = false;
};
