// platform.h - the few things each platform (Windows now, the stick later)
// must provide to the game. Everything else is shared.
#pragma once
#include <stdint.h>

struct Tone {
    uint16_t freqHz;   // 0 = rest
    uint16_t ms;
};

// Start playing a sound and return immediately (the game keeps running).
// The platform copies the tones, and a new sound replaces one still playing.
void platformPlaySound(const Tone* tones, uint8_t count);

// High score storage (a file on Windows, flash on the stick).
uint32_t platformLoadBest();
void     platformSaveBest(uint32_t best);
