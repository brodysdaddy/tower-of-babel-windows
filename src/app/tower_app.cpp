#include "tower_app.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

constexpr int   SW = Framebuffer::W;
constexpr int   SH = Framebuffer::H;
constexpr float LH = TowerGame::LAYER_H;
constexpr float GROUND_Y   = 214.0f;  // screen row of the ground before scrolling
constexpr float SLIDER_MIN = 84.0f;   // camera keeps the moving block at least this low
constexpr float GRAVITY    = 520.0f;  // px/s^2 for falling pieces
constexpr float PANEL_DELAY = 0.6f;   // game over panel appears after this
constexpr float INPUT_LOCK  = 1.0f;   // ...and accepts buttons after this

// ---- Sounds (Hz, ms) ------------------------------------------------------
const Tone SND_CLICK[] = { {1800, 15} };
const Tone SND_THUD[]  = { {300, 35}, {200, 45} };
const Tone SND_MISS[]  = { {392, 150}, {370, 150}, {349, 150}, {330, 380} };
const Tone SND_BEST[]  = { {523, 90}, {659, 90}, {784, 90}, {1047, 280} };

// ---- Sky: colors at each altitude (scroll px), top and bottom of the screen
const float SKY_ALT[] = { 0, 200, 480, 900 };
const Rgb   SKY_TOP[] = { {88, 58, 140}, {44, 32, 96}, {10, 10, 34}, {2, 2, 12} };
const Rgb   SKY_BOT[] = { {255, 158, 92}, {196, 92, 124}, {46, 34, 86}, {12, 12, 44} };
constexpr int SKY_STOPS = 4;

// ---- Brick colors: the material changes every 10 layers -------------------
const Rgb STONE = { 150, 140, 125 };   // the foundation
const Rgb TIERS[] = {
    {198, 146, 90},   // mud brick
    {186, 98, 62},    // fired terracotta
    {52, 98, 178},    // glazed blue
    {40, 150, 150},   // turquoise
    {222, 182, 74},   // gold
};
constexpr int NUM_TIERS = sizeof(TIERS) / sizeof(TIERS[0]);

const Rgb WHITE = {255, 255, 255};
const Rgb GOLD  = {255, 222, 110};
const Rgb INK   = {30, 16, 24};

float clamp01(float t) { return t < 0 ? 0 : (t > 1 ? 1 : t); }
int   clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
uint32_t hash32(uint32_t x) {
    x *= 2654435761u;
    x ^= x >> 15;
    x *= 2246822519u;
    x ^= x >> 13;
    return x;
}

void skyColors(float alt, Rgb& top, Rgb& bot) {
    if (alt <= SKY_ALT[0]) { top = SKY_TOP[0]; bot = SKY_BOT[0]; return; }
    for (int i = 1; i < SKY_STOPS; ++i) {
        if (alt < SKY_ALT[i]) {
            float t = (alt - SKY_ALT[i - 1]) / (SKY_ALT[i] - SKY_ALT[i - 1]);
            top = lerpRgb(SKY_TOP[i - 1], SKY_TOP[i], t);
            bot = lerpRgb(SKY_BOT[i - 1], SKY_BOT[i], t);
            return;
        }
    }
    top = SKY_TOP[SKY_STOPS - 1];
    bot = SKY_BOT[SKY_STOPS - 1];
}

void ziggurat(Framebuffer& fb, int x, int baseY, int w, int steps, uint16_t c) {
    const int stepH = 5;
    for (int s = 0; s < steps; ++s) {
        int inset = s * 5;
        fb.fillRect(x + inset, baseY - (s + 1) * stepH, w - 2 * inset, stepH, c);
    }
}

}  // namespace

// ===========================================================================

TowerApp::TowerApp(uint32_t seed, const char* frontName, const char* sideName, bool hasExit)
    : rng_(seed ? seed : 0x1234567u), hasExit_(hasExit) {
    snprintf(frontName_, sizeof frontName_, "%s", frontName);
    snprintf(sideName_, sizeof sideName_, "%s", sideName);
    best_ = platformLoadBest();
    memset(debris_, 0, sizeof debris_);
    memset(parts_, 0, sizeof parts_);
    for (Cloud& c : clouds_) {
        c.x = randRange(-20, SW + 20);
        c.sy0 = randRange(-10, 150);
        c.speed = randRange(3, 9);
        c.r = randRange(6, 11);
    }
    for (Star& s : stars_) {
        s.x = (uint8_t)randRange(0, SW - 1);
        s.y = (uint8_t)randRange(0, SH - 1);
        s.big = rand01() < 0.15f;
        s.phase = randRange(0, 6.28f);
    }
}

float TowerApp::rand01() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return (rng_ >> 8) * (1.0f / 16777216.0f);
}

void TowerApp::play(const Tone* notes, uint8_t count) { platformPlaySound(notes, count); }

// Each perfect in a streak chimes one semitone higher.
void TowerApp::playPerfect(int combo) {
    int semis = combo - 1;
    if (semis > 12) semis = 12;
    float f = 784.0f * powf(2.0f, semis / 12.0f);
    Tone n[2] = { {(uint16_t)f, 60}, {(uint16_t)(f * 1.5f), 110} };
    play(n, 2);
}

float TowerApp::screenY(float worldY) const { return GROUND_Y - worldY + cam_ + shakeY_; }
float TowerApp::layerTopY(int i) const { return screenY((i + 1) * LH); }

Rgb TowerApp::layerColor(int i) {
    if (i == 0) return STONE;
    int tier = (i - 1) / 10;
    if (tier >= NUM_TIERS) tier = NUM_TIERS - 1;
    int variation = (int)(hash32((uint32_t)i) % 17) - 8;   // slight per-row tint
    return shade(TIERS[tier], variation);
}

// ===========================================================================
// Update

void TowerApp::pause() {
    if (mode_ == Mode::Playing) { mode_ = Mode::Paused; modeTime_ = 0; }
}

void TowerApp::enterTitle() {
    game_.reset();
    mode_ = Mode::Title;
    modeTime_ = 0;
    exitRequested_ = false;
    popupLife_ = 0;
}

void TowerApp::startGame() {
    game_.reset();
    mode_ = Mode::Playing;
    modeTime_ = 0;
    newBest_ = false;
    popupLife_ = 0;
}

void TowerApp::update(float dt, bool front, bool side) {
    time_ += dt;
    modeTime_ += dt;
    switch (mode_) {
    case Mode::Title:
        game_.update(dt);   // the block slides behind the title as a demo
        if (front) { play(SND_CLICK, 1); startGame(); }
        else if (side && hasExit_) { play(SND_CLICK, 1); exitRequested_ = true; }
        break;
    case Mode::Playing:
        if (side) { play(SND_CLICK, 1); pause(); return; }
        // Drop first, so the block lands exactly where the player saw it.
        if (front) handleDrop();
        if (mode_ == Mode::Playing) game_.update(dt);
        break;
    case Mode::Paused:
        if (front || side) { play(SND_CLICK, 1); mode_ = Mode::Playing; modeTime_ = 0; }
        return;   // everything stays frozen while paused
    case Mode::GameOver:
        if (modeTime_ > INPUT_LOCK) {
            if (front) { play(SND_CLICK, 1); startGame(); }
            else if (side) { play(SND_CLICK, 1); game_.reset(); mode_ = Mode::Title; modeTime_ = 0; }
        }
        break;
    }
    updateEffects(dt);
}

void TowerApp::handleDrop() {
    DropResult r = game_.drop();
    if (r == DropResult::Miss) { gameOver(); return; }

    int n = game_.layerCount();
    const Slab& placed = game_.layer(n - 1);
    float bottom = (n - 1) * LH, top = n * LH;

    if (r == DropResult::Perfect) {
        playPerfect(game_.combo());
        for (int k = 0; k < 16; ++k) {
            spawnParticle(placed.x + rand01() * placed.w, top, randRange(-35, 35),
                          randRange(30, 95), randRange(0.5f, 0.9f), 90,
                          rand01() < 0.5f ? GOLD : WHITE, rand01() < 0.3f ? 2 : 1);
        }
        snprintf(popup_, sizeof popup_, "PERFECT!");
        if (game_.combo() >= 2) snprintf(popup2_, sizeof popup2_, "X%d COMBO", game_.combo());
        else popup2_[0] = 0;
        popupLife_ = 0.9f;
        popupY_ = layerTopY(n - 1) - 26;
        shake_ = 1.5f;
        flash_ = 0.3f;
    } else {
        play(SND_THUD, 2);
        const Slab& oh = game_.lastOverhang();
        spawnDebris(oh, n - 1, oh.x < placed.x ? -1.0f : 1.0f);
        Rgb dust = shade(layerColor(n - 1), 40);
        for (int k = 0; k < 10; ++k) {
            bool leftSide = k < 5;
            float x = leftSide ? placed.x : placed.x + placed.w;
            spawnParticle(x, bottom + 1, (leftSide ? -1 : 1) * randRange(10, 40),
                          randRange(5, 35), randRange(0.3f, 0.6f), 120, dust,
                          rand01() < 0.4f ? 2 : 1);
        }
        shake_ = 2.5f;
    }
}

void TowerApp::gameOver() {
    const Slab& s = game_.lastOverhang();   // the whole block falls
    const Slab& top = game_.layer(game_.layerCount() - 1);
    float dir = (s.x + s.w / 2) < (top.x + top.w / 2) ? -1.0f : 1.0f;
    spawnDebris(s, game_.layerCount(), dir);
    shake_ = 5;
    uint32_t score = (uint32_t)game_.score();
    if (score > best_) {
        best_ = score;
        newBest_ = true;
        platformSaveBest(best_);
        play(SND_BEST, 4);
    } else {
        play(SND_MISS, 4);
    }
    mode_ = Mode::GameOver;
    modeTime_ = 0;
}

void TowerApp::spawnDebris(const Slab& s, int layerIndex, float dir) {
    for (Debris& d : debris_) {
        if (d.active) continue;
        d.active = true;
        d.cx = s.x + s.w / 2;
        d.cy = layerIndex * LH + LH / 2;
        d.w = s.w;
        d.h = LH;
        d.vx = dir * randRange(15, 35);
        d.vy = randRange(20, 45);
        d.angle = 0;
        d.spin = dir * randRange(2.0f, 4.0f);
        d.color = layerColor(layerIndex);
        return;
    }
}

void TowerApp::spawnParticle(float x, float y, float vx, float vy, float life, float gravity,
                             Rgb color, uint8_t size) {
    for (Particle& p : parts_) {
        if (p.life > 0) continue;
        p = { x, y, vx, vy, life, life, gravity, color, size };
        return;
    }
}

void TowerApp::updateEffects(float dt) {
    // Camera eases up so the moving block never gets too close to the top.
    float target = (game_.layerCount() + 1) * LH - (GROUND_Y - SLIDER_MIN);
    if (target < 0) target = 0;
    float k = dt * 5.0f;
    cam_ += (target - cam_) * (k > 1 ? 1 : k);

    shake_ *= expf(-dt * 10.0f);
    if (shake_ < 0.1f) shake_ = 0;
    shakeY_ = shake_ * sinf(time_ * 70.0f);

    if (flash_ > 0) flash_ -= dt;
    if (popupLife_ > 0) { popupLife_ -= dt; popupY_ -= 22 * dt; }

    for (Debris& d : debris_) {
        if (!d.active) continue;
        d.vy -= GRAVITY * dt;
        d.cx += d.vx * dt;
        d.cy += d.vy * dt;
        d.angle += d.spin * dt;
        if (screenY(d.cy) > SH + 40) d.active = false;
    }
    for (Particle& p : parts_) {
        if (p.life <= 0) continue;
        p.life -= dt;
        p.vy -= p.gravity * dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
    }
    // Clouds drift sideways and pass by as you climb (parallax 0.7).
    for (Cloud& c : clouds_) {
        c.x += c.speed * dt;
        if (c.x > SW + 30) c.x = -30;
        if (c.sy0 + cam_ * 0.7f > SH + 30) {
            c.sy0 = -30 - cam_ * 0.7f - randRange(0, 60);
            c.x = randRange(-20, SW + 20);
        }
    }
}

// ===========================================================================
// Drawing

void TowerApp::render(Framebuffer& fb) {
    drawBackground(fb);

    int n = game_.layerCount();
    for (int i = n - TowerGame::HISTORY; i < n; ++i) {
        if (!game_.hasLayer(i)) continue;
        float top = layerTopY(i);
        if (top > SH || top + LH < 0) continue;
        drawSlab(fb, i, game_.layer(i), top, i == n - 1 ? flash_ : 0);
    }
    if (mode_ != Mode::GameOver) drawSlab(fb, n, game_.slider(), layerTopY(n), 0);

    drawEffects(fb);
    drawHud(fb);
    drawOverlay(fb);
}

void TowerApp::drawBackground(Framebuffer& fb) {
    // Sky gradient: sunset at the bottom, night as the tower climbs.
    Rgb top, bot;
    skyColors(cam_, top, bot);
    for (int y = 0; y < SH; ++y) {
        skyRow_[y] = lerpRgb(top, bot, y / (float)(SH - 1));
        fb.hline(0, y, SW, to565(skyRow_[y]));
    }

    // Stars and moon fade in with altitude.
    float starA = clamp01((cam_ - 120.0f) / 260.0f);
    if (starA > 0) {
        int scroll = (int)(cam_ * 0.05f);
        for (const Star& s : stars_) {
            int sy = (s.y + scroll) % SH;
            float tw = 0.55f + 0.45f * sinf(time_ * 2.5f + s.phase);
            int a = (int)(starA * tw * 255);
            fb.blend(s.x, sy, to565(WHITE), a);
            if (s.big) {
                fb.blend(s.x - 1, sy, to565(WHITE), a / 2);
                fb.blend(s.x + 1, sy, to565(WHITE), a / 2);
                fb.blend(s.x, sy - 1, to565(WHITE), a / 2);
                fb.blend(s.x, sy + 1, to565(WHITE), a / 2);
            }
        }
        // Crescent moon: inside the moon circle but outside an offset "bite" circle.
        int a = (int)(starA * 255);
        const int mx = 104, my = 36, r = 9, bx = mx + 4, by = my - 3, br = 8;
        for (int y = my - r; y <= my + r; ++y)
            for (int x = mx - r; x <= mx + r; ++x) {
                bool inMoon = (x - mx) * (x - mx) + (y - my) * (y - my) <= r * r;
                bool inBite = (x - bx) * (x - bx) + (y - by) * (y - by) <= br * br;
                if (inMoon && !inBite) fb.blend(x, y, rgb565(245, 240, 210), a);
            }
    }

    // Sun sinks below the horizon as you climb (parallax 0.55).
    float sunY = GROUND_Y - 26 + cam_ * 0.55f;
    if (sunY - 30 < SH) {
        fb.blendCircle(42, (int)sunY, 30, rgb565(255, 200, 120), 45);
        fb.fillCircle(42, (int)sunY, 19, rgb565(255, 224, 150));
    }

    // Clouds take their color from the sky behind them.
    for (const Cloud& c : clouds_) {
        int sy = (int)(c.sy0 + cam_ * 0.7f);
        if (sy < -20 || sy > SH + 20) continue;
        Rgb base = skyRow_[clampi(sy, 0, SH - 1)];
        uint16_t col = to565(lerpRgb(base, {255, 244, 236}, 0.38f));
        int r = (int)c.r, x = (int)c.x;
        fb.fillCircle(x, sy, r, col);
        fb.fillCircle(x + (int)(r * 0.9f), sy + 2, (int)(r * 0.75f), col);
        fb.fillCircle(x - (int)(r * 0.9f), sy + 3, (int)(r * 0.65f), col);
    }

    // Distant land with ziggurat silhouettes (parallax 0.45).
    int hy = (int)(GROUND_Y - 4 + cam_ * 0.45f);
    if (hy - 25 < SH) {
        uint16_t land = to565(lerpRgb(bot, {48, 26, 52}, 0.65f));
        fb.fillRect(0, hy, SW, SH - hy, land);
        ziggurat(fb, 6, hy, 44, 4, land);
        ziggurat(fb, 96, hy, 32, 3, land);
    }

    // Near ground: sand.
    int gy = (int)(GROUND_Y + cam_ + shakeY_);
    if (gy < SH) {
        fb.fillRect(0, gy, SW, SH - gy, rgb565(206, 164, 104));
        fb.hline(0, gy, SW, rgb565(232, 196, 136));
        for (uint32_t k = 0; k < 28; ++k) {
            uint32_t h = hash32(k + 77);
            fb.pixel((int)(h % SW), gy + 3 + (int)((h >> 8) % 22), rgb565(176, 134, 82));
        }
    }
}

// One layer: two courses of bricks with mortar lines, lit from above.
void TowerApp::drawSlab(Framebuffer& fb, int i, const Slab& s, float topY, float flash) {
    int x0 = (int)floorf(s.x + 0.5f);
    int x1 = (int)floorf(s.x + s.w + 0.5f);
    int w = x1 - x0;
    int y = (int)floorf(topY + 0.5f);
    if (w <= 0) return;
    Rgb c = layerColor(i);
    uint16_t mortar = to565(shade(c, -45));
    fb.fillRect(x0, y, w, (int)LH, to565(c));
    fb.hline(x0, y, w, to565(shade(c, 35)));
    fb.hline(x0, y + 6, w, mortar);
    fb.hline(x0, y + (int)LH - 1, w, to565(shade(c, -30)));

    // Vertical joints are aligned to the world, so a cut slices through
    // bricks naturally. Courses alternate offsets (running bond).
    const int BRICK = 14;
    for (int k = 0; k < 2; ++k) {
        int off = ((i * 2 + k) % 2) * (BRICK / 2);
        int rowY = y + 1 + k * 6;
        int rowH = k == 0 ? 5 : 4;
        int m = ((x0 - off) % BRICK + BRICK) % BRICK;
        for (int jx = x0 - m + BRICK; jx < x1 - 1; jx += BRICK) fb.vline(jx, rowY, rowH, mortar);
    }
    fb.vline(x0, y, (int)LH, to565(shade(c, -15)));
    fb.vline(x1 - 1, y, (int)LH, to565(shade(c, -35)));

    if (flash > 0) fb.blendRect(x0, y, w, (int)LH, to565(WHITE), (int)(clamp01(flash / 0.3f) * 200));
}

void TowerApp::drawEffects(Framebuffer& fb) {
    for (const Debris& d : debris_) {
        if (!d.active) continue;
        float cs = cosf(d.angle), sn = sinf(d.angle);
        float hx = d.w / 2, hy = d.h / 2;
        float cx = d.cx, cy = screenY(d.cy);
        const float lx[4] = { -hx, hx, hx, -hx };
        const float ly[4] = { -hy, -hy, hy, hy };
        float xs[4], ys[4];
        for (int k = 0; k < 4; ++k) {
            xs[k] = cx + lx[k] * cs - ly[k] * sn;
            ys[k] = cy + lx[k] * sn + ly[k] * cs;
        }
        fb.fillQuad(xs, ys, to565(d.color));
    }
    for (const Particle& p : parts_) {
        if (p.life <= 0) continue;
        int a = (int)(clamp01(p.life / p.maxLife) * 255);
        fb.blendRect((int)p.x, (int)screenY(p.y), p.size, p.size, to565(p.color), a);
    }
    if (popupLife_ > 0) {
        int a = (int)(clamp01(popupLife_ / 0.3f) * 255);
        drawCentered(fb, (int)popupY_, popup_, GOLD, 2, a);
        if (popup2_[0]) drawCentered(fb, (int)popupY_ + 17, popup2_, WHITE, 1, a);
    }
}

void TowerApp::drawCentered(Framebuffer& fb, int y, const char* s, Rgb c, int scale, int alpha) {
    int x = (SW - Framebuffer::textWidth(s, scale)) / 2;
    fb.text(x + 1, y + 1, s, to565(INK), scale, alpha * 2 / 3);   // drop shadow
    fb.text(x, y, s, to565(c), scale, alpha);
}

void TowerApp::drawHud(Framebuffer& fb) {
    if (mode_ == Mode::Title) return;
    if (mode_ == Mode::GameOver && modeTime_ >= PANEL_DELAY) return;   // the panel shows it
    char buf[16];
    snprintf(buf, sizeof buf, "%d", game_.score());
    drawCentered(fb, 14, buf, WHITE, 3);
    snprintf(buf, sizeof buf, "BEST %u", (unsigned)best_);
    fb.text(3, 3, buf, to565(shade(WHITE, -60)), 1);
}

void TowerApp::drawOverlay(Framebuffer& fb) {
    char buf[24];
    bool blink = ((int)(time_ * 2.5f) % 2) == 0;
    switch (mode_) {
    case Mode::Title:
        fb.blendRect(0, 24, SW, 56, rgb565(0, 0, 0), 70);
        drawCentered(fb, 30, "TOWER", GOLD, 3);
        drawCentered(fb, 58, "OF BABEL", GOLD, 2);
        drawCentered(fb, 98, "STACK IT HIGH", WHITE, 1);
        if (blink) {
            snprintf(buf, sizeof buf, "%s: BUILD", frontName_);
            drawCentered(fb, 120, buf, WHITE, 1);
        }
        if (best_ > 0) {
            snprintf(buf, sizeof buf, "BEST %u", (unsigned)best_);
            drawCentered(fb, 136, buf, GOLD, 1);
        }
        if (hasExit_) {
            snprintf(buf, sizeof buf, "%s: BACK", sideName_);
            drawCentered(fb, 152, buf, shade(WHITE, -60), 1);
        }
        break;
    case Mode::Paused:
        fb.blendRect(0, 0, SW, SH, rgb565(0, 0, 0), 130);
        drawCentered(fb, 100, "PAUSED", WHITE, 2);
        snprintf(buf, sizeof buf, "%s: RESUME", frontName_);
        drawCentered(fb, 124, buf, WHITE, 1);
        break;
    case Mode::GameOver: {
        if (modeTime_ < PANEL_DELAY) break;
        const int px = 10, py = 70, pw = SW - 20, ph = 114;
        fb.blendRect(px, py, pw, ph, rgb565(12, 8, 22), 190);
        uint16_t edge = to565(shade(GOLD, -70));
        fb.hline(px, py, pw, edge);
        fb.hline(px, py + ph - 1, pw, edge);
        fb.vline(px, py, ph, edge);
        fb.vline(px + pw - 1, py, ph, edge);
        drawCentered(fb, py + 8, "GAME OVER", WHITE, 2);
        drawCentered(fb, py + 30, "HEIGHT", shade(WHITE, -50), 1);
        snprintf(buf, sizeof buf, "%d", game_.score());
        drawCentered(fb, py + 40, buf, GOLD, 3);
        if (newBest_) { if (blink) drawCentered(fb, py + 66, "NEW BEST!", GOLD, 1); }
        else {
            snprintf(buf, sizeof buf, "BEST %u", (unsigned)best_);
            drawCentered(fb, py + 66, buf, WHITE, 1);
        }
        snprintf(buf, sizeof buf, "PERFECTS %d", game_.perfects());
        drawCentered(fb, py + 77, buf, shade(WHITE, -50), 1);
        if (modeTime_ > INPUT_LOCK) {
            snprintf(buf, sizeof buf, "%s: AGAIN", frontName_);
            drawCentered(fb, py + 92, buf, WHITE, 1);
            snprintf(buf, sizeof buf, "%s: MENU", sideName_);
            drawCentered(fb, py + 102, buf, shade(WHITE, -50), 1);
        }
        break;
    }
    case Mode::Playing:
        break;
    }
}
