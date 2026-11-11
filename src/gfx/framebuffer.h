// framebuffer.h - a 135 x 240 RGB565 pixel buffer with simple drawing tools.
//
// RGB565 (16-bit color) is the M5StickS3 display's native format, so the
// whole game draws here and each platform only has to show the finished
// buffer: Windows scales it into a window; the stick can push it straight to
// its screen. Portable: no heap, no platform code.
#pragma once
#include <stdint.h>

struct Rgb { uint8_t r, g, b; };

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
inline uint16_t to565(Rgb c) { return rgb565(c.r, c.g, c.b); }
Rgb lerpRgb(Rgb a, Rgb b, float t);   // t = 0..1
Rgb shade(Rgb c, int delta);          // brighten (+) or darken (-)

class Framebuffer {
public:
    static constexpr int W = 135;
    static constexpr int H = 240;
    uint16_t pixels[W * H];

    void fill(uint16_t c);
    void pixel(int x, int y, uint16_t c);
    void blend(int x, int y, uint16_t c, int alpha);   // alpha 0..255
    void hline(int x, int y, int w, uint16_t c);
    void vline(int x, int y, int h, uint16_t c);
    void fillRect(int x, int y, int w, int h, uint16_t c);
    void blendRect(int x, int y, int w, int h, uint16_t c, int alpha);
    void fillCircle(int cx, int cy, int r, uint16_t c);
    void blendCircle(int cx, int cy, int r, uint16_t c, int alpha);
    // Convex 4-corner shape (used for tumbling pieces).
    void fillQuad(const float xs[4], const float ys[4], uint16_t c);

    // Built-in 5x7 font: digits, A-Z (lowercase shown as uppercase), ! . : - / ? % +
    void text(int x, int y, const char* s, uint16_t c, int scale = 1, int alpha = 255);
    static int textWidth(const char* s, int scale = 1);
};
