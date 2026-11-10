// tower_game.h - rules for "Tower of Babel", a two-button stacking game.
//
// A block slides back and forth above the tower. Drop it: whatever hangs
// past the block below is sliced off, so the tower gets narrower. Land it
// (almost) exactly on top for a PERFECT; two or more perfects in a row grow
// the block back a little. Miss completely and the game is over.
//
// Portable like the trivia core: no heap, no I/O, no platform code.
// Units are screen pixels (the stick's portrait screen is 135 x 240), so the
// Windows prototype and the stick share one coordinate system.
#pragma once
#include <stdint.h>

struct Slab {
    float x;  // left edge
    float w;  // width
};

enum class DropResult : uint8_t { Placed, Perfect, Miss };

class TowerGame {
public:
    // ---- Tuning knobs -------------------------------------------------------
    static constexpr float WORLD_W           = 135.0f;  // play area width
    static constexpr float LAYER_H           = 12.0f;   // height of one layer
    static constexpr float START_W           = 80.0f;   // starting block width
    static constexpr float PERFECT_TOLERANCE = 3.0f;    // px off that still counts
    static constexpr float PERFECT_GROW      = 4.0f;    // growth per combo perfect
    static constexpr float START_SPEED       = 55.0f;   // px per second
    static constexpr float SPEED_STEP        = 3.0f;    // added after every drop
    static constexpr float MAX_SPEED         = 190.0f;
    // Only the most recent layers are kept (that's all the screen can show).
    static constexpr int   HISTORY           = 32;

    TowerGame();
    void reset();
    void update(float dt);   // slide the moving block
    DropResult drop();

    bool  isOver() const     { return over_; }
    int   score() const      { return count_ - 1; }   // layers above the base
    int   layerCount() const { return count_; }       // including the base
    bool  hasLayer(int i) const { return i >= 0 && i < count_ && i >= count_ - HISTORY; }
    const Slab& layer(int i) const { return layers_[i % HISTORY]; }
    const Slab& slider() const   { return slider_; }   // the moving block
    int   combo() const      { return combo_; }
    int   perfects() const   { return perfects_; }
    // The piece cut off by the last drop (the whole block on a miss).
    const Slab& lastOverhang() const { return overhang_; }

private:
    void  spawnSlider();
    float minX() const { return -slider_.w * 0.35f; }
    float maxX() const { return WORLD_W - slider_.w * 0.65f; }

    Slab  layers_[HISTORY];
    int   count_ = 0;
    Slab  slider_{};
    float dir_ = 1.0f;
    float speed_ = START_SPEED;
    bool  fromLeft_ = true;
    int   combo_ = 0;
    int   perfects_ = 0;
    bool  over_ = false;
    Slab  overhang_{};
};
