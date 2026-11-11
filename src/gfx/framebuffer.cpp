#include "framebuffer.h"
#include <math.h>
#include <string.h>

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

Rgb lerpRgb(Rgb a, Rgb b, float t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return { (uint8_t)(a.r + (b.r - a.r) * t + 0.5f),
             (uint8_t)(a.g + (b.g - a.g) * t + 0.5f),
             (uint8_t)(a.b + (b.b - a.b) * t + 0.5f) };
}

Rgb shade(Rgb c, int d) {
    return { (uint8_t)clampi(c.r + d, 0, 255), (uint8_t)clampi(c.g + d, 0, 255),
             (uint8_t)clampi(c.b + d, 0, 255) };
}

static inline uint16_t blend565(uint16_t d, uint16_t s, int a) {
    int dr = d >> 11, dg = (d >> 5) & 63, db = d & 31;
    int sr = s >> 11, sg = (s >> 5) & 63, sb = s & 31;
    dr += (sr - dr) * a / 255;
    dg += (sg - dg) * a / 255;
    db += (sb - db) * a / 255;
    return (uint16_t)((dr << 11) | (dg << 5) | db);
}

void Framebuffer::fill(uint16_t c) {
    for (int i = 0; i < W * H; ++i) pixels[i] = c;
}

void Framebuffer::pixel(int x, int y, uint16_t c) {
    if (x >= 0 && x < W && y >= 0 && y < H) pixels[y * W + x] = c;
}

void Framebuffer::blend(int x, int y, uint16_t c, int alpha) {
    if (alpha <= 0 || x < 0 || x >= W || y < 0 || y >= H) return;
    uint16_t& p = pixels[y * W + x];
    p = alpha >= 255 ? c : blend565(p, c, alpha);
}

void Framebuffer::hline(int x, int y, int w, uint16_t c) {
    if (y < 0 || y >= H) return;
    int x0 = clampi(x, 0, W), x1 = clampi(x + w, 0, W);
    for (int i = x0; i < x1; ++i) pixels[y * W + i] = c;
}

void Framebuffer::vline(int x, int y, int h, uint16_t c) {
    if (x < 0 || x >= W) return;
    int y0 = clampi(y, 0, H), y1 = clampi(y + h, 0, H);
    for (int j = y0; j < y1; ++j) pixels[j * W + x] = c;
}

void Framebuffer::fillRect(int x, int y, int w, int h, uint16_t c) {
    int x0 = clampi(x, 0, W), x1 = clampi(x + w, 0, W);
    int y0 = clampi(y, 0, H), y1 = clampi(y + h, 0, H);
    for (int j = y0; j < y1; ++j)
        for (int i = x0; i < x1; ++i) pixels[j * W + i] = c;
}

void Framebuffer::blendRect(int x, int y, int w, int h, uint16_t c, int alpha) {
    int x0 = clampi(x, 0, W), x1 = clampi(x + w, 0, W);
    int y0 = clampi(y, 0, H), y1 = clampi(y + h, 0, H);
    for (int j = y0; j < y1; ++j)
        for (int i = x0; i < x1; ++i) pixels[j * W + i] = blend565(pixels[j * W + i], c, alpha);
}

void Framebuffer::fillCircle(int cx, int cy, int r, uint16_t c) {
    for (int dy = -r; dy <= r; ++dy) {
        int dx = (int)sqrtf((float)(r * r - dy * dy));
        hline(cx - dx, cy + dy, 2 * dx + 1, c);
    }
}

void Framebuffer::blendCircle(int cx, int cy, int r, uint16_t c, int alpha) {
    for (int dy = -r; dy <= r; ++dy) {
        int dx = (int)sqrtf((float)(r * r - dy * dy));
        for (int x = cx - dx; x <= cx + dx; ++x) blend(x, cy + dy, c, alpha);
    }
}

void Framebuffer::fillQuad(const float xs[4], const float ys[4], uint16_t c) {
    float minx = xs[0], maxx = xs[0], miny = ys[0], maxy = ys[0];
    for (int k = 1; k < 4; ++k) {
        if (xs[k] < minx) minx = xs[k];
        if (xs[k] > maxx) maxx = xs[k];
        if (ys[k] < miny) miny = ys[k];
        if (ys[k] > maxy) maxy = ys[k];
    }
    if (maxx < 0 || minx >= W || maxy < 0 || miny >= H) return;
    int x0 = clampi((int)floorf(minx), 0, W - 1), x1 = clampi((int)ceilf(maxx), 0, W - 1);
    int y0 = clampi((int)floorf(miny), 0, H - 1), y1 = clampi((int)ceilf(maxy), 0, H - 1);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            float px = x + 0.5f, py = y + 0.5f;
            bool pos = false, neg = false;
            for (int k = 0; k < 4; ++k) {
                int j = (k + 1) & 3;
                float cross = (xs[j] - xs[k]) * (py - ys[k]) - (ys[j] - ys[k]) * (px - xs[k]);
                if (cross > 0) pos = true; else if (cross < 0) neg = true;
            }
            if (!(pos && neg)) pixels[y * W + x] = c;
        }
    }
}

// ---- 5x7 font: 7 rows per glyph, bit 4 = leftmost column ------------------
static const char FONT_CHARS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!.:-/?%+";
static const uint8_t FONT[][7] = {
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, // 0 1
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}, // 2 3
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, // 4 5
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, // 6 7
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, // 8 9
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, // A B
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}, // C D
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, // E F
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, // G H
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, // I J
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, // K L
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, {0x11,0x11,0x19,0x15,0x13,0x11,0x11}, // M N
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, // O P
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, // Q R
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, // S T
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, // U V
    {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}, {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, // W X
    {0x11,0x11,0x11,0x0A,0x04,0x04,0x04}, {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}, // Y Z
    {0x04,0x04,0x04,0x04,0x04,0x00,0x04}, {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}, // ! .
    {0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00}, {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}, // : -
    {0x00,0x01,0x02,0x04,0x08,0x10,0x00}, {0x0E,0x11,0x01,0x02,0x04,0x00,0x04}, // / ?
    {0x18,0x19,0x02,0x04,0x08,0x13,0x03}, {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}, // % +
};
static_assert(sizeof(FONT) / sizeof(FONT[0]) == sizeof(FONT_CHARS) - 1, "font table mismatch");

static const uint8_t* glyph(char ch) {
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    for (int i = 0; FONT_CHARS[i]; ++i)
        if (FONT_CHARS[i] == ch) return FONT[i];
    return nullptr;   // space or unknown: blank
}

void Framebuffer::text(int x, int y, const char* s, uint16_t c, int scale, int alpha) {
    for (; *s; ++s, x += 6 * scale) {
        const uint8_t* g = glyph(*s);
        if (!g) continue;
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col)
                if (g[row] & (0x10 >> col)) {
                    if (alpha >= 255) fillRect(x + col * scale, y + row * scale, scale, scale, c);
                    else blendRect(x + col * scale, y + row * scale, scale, scale, c, alpha);
                }
    }
}

int Framebuffer::textWidth(const char* s, int scale) {
    int n = (int)strlen(s);
    return n ? (n * 6 - 1) * scale : 0;
}
