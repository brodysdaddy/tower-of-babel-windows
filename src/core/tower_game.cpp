#include "tower_game.h"

static float maxf(float a, float b) { return a > b ? a : b; }
static float minf(float a, float b) { return a < b ? a : b; }
static float absf(float a) { return a < 0 ? -a : a; }

TowerGame::TowerGame() { reset(); }

void TowerGame::reset() {
    layers_[0] = { (WORLD_W - START_W) / 2.0f, START_W };
    count_ = 1;
    speed_ = START_SPEED;
    combo_ = 0;
    perfects_ = 0;
    over_ = false;
    fromLeft_ = true;
    overhang_ = {};
    spawnSlider();
}

// New block, same width as the top of the tower, entering from alternating sides.
void TowerGame::spawnSlider() {
    slider_.w = layer(count_ - 1).w;
    if (fromLeft_) { slider_.x = minX(); dir_ = 1.0f; }
    else           { slider_.x = maxX(); dir_ = -1.0f; }
    fromLeft_ = !fromLeft_;
}

void TowerGame::update(float dt) {
    if (over_) return;
    slider_.x += dir_ * speed_ * dt;
    // Bounce off the travel limits.
    if (slider_.x > maxX())      { slider_.x = 2 * maxX() - slider_.x; dir_ = -1.0f; }
    else if (slider_.x < minX()) { slider_.x = 2 * minX() - slider_.x; dir_ = 1.0f; }
}

DropResult TowerGame::drop() {
    if (over_) return DropResult::Miss;
    const Slab top = layer(count_ - 1);
    float left  = maxf(slider_.x, top.x);
    float right = minf(slider_.x + slider_.w, top.x + top.w);

    if (right - left < 1.0f) {               // missed the tower entirely
        over_ = true;
        overhang_ = slider_;
        combo_ = 0;
        return DropResult::Miss;
    }

    Slab placed;
    DropResult result;
    if (absf(slider_.x - top.x) <= PERFECT_TOLERANCE) {
        // Perfect: snap onto the block below; a streak grows it back.
        ++combo_;
        ++perfects_;
        placed = top;
        if (combo_ >= 2 && placed.w < START_W) {
            float nw = minf(START_W, placed.w + PERFECT_GROW);
            placed.x -= (nw - placed.w) / 2.0f;
            placed.w = nw;
        }
        overhang_ = {};
        result = DropResult::Perfect;
    } else {
        // Keep the overlap; the rest is cut off (only one side can overhang,
        // because the moving block is exactly as wide as the one below).
        combo_ = 0;
        placed = { left, right - left };
        if (slider_.x < top.x) overhang_ = { slider_.x, top.x - slider_.x };
        else                   overhang_ = { right, slider_.x + slider_.w - right };
        result = DropResult::Placed;
    }

    layers_[count_ % HISTORY] = placed;
    ++count_;
    speed_ = minf(MAX_SPEED, speed_ + SPEED_STEP);
    spawnSlider();
    return result;
}
