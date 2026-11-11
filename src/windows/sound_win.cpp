// sound_win.cpp - non-blocking sound for Windows.
// Beep() blocks until the note ends, so notes play on a background thread
// and the game never pauses for a sound. A new sound cuts off the old one
// (between notes).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include "../app/platform.h"

bool g_muted = false;

namespace {
const int MAX_NOTES = 16;
std::mutex              g_lock;
std::condition_variable g_wake;
Tone g_pending[MAX_NOTES];
int  g_pendingCount = 0;
bool g_hasPending = false;
bool g_started = false;

void worker() {
    Tone notes[MAX_NOTES];
    for (;;) {
        int count;
        {
            std::unique_lock<std::mutex> lk(g_lock);
            g_wake.wait(lk, [] { return g_hasPending; });
            count = g_pendingCount;
            for (int i = 0; i < count; ++i) notes[i] = g_pending[i];
            g_hasPending = false;
        }
        for (int i = 0; i < count; ++i) {
            {
                std::lock_guard<std::mutex> lk(g_lock);
                if (g_hasPending) break;   // a newer sound replaces this one
            }
            if (notes[i].freqHz < 37) Sleep(notes[i].ms);   // rest (Beep's minimum is 37 Hz)
            else Beep(notes[i].freqHz, notes[i].ms);
        }
    }
}
}  // namespace

void platformPlaySound(const Tone* notes, uint8_t count) {
    if (g_muted || count == 0) return;
    std::lock_guard<std::mutex> lk(g_lock);
    if (!g_started) {
        std::thread(worker).detach();
        g_started = true;
    }
    if (count > MAX_NOTES) count = MAX_NOTES;
    for (int i = 0; i < count; ++i) g_pending[i] = notes[i];
    g_pendingCount = count;
    g_hasPending = true;
    g_wake.notify_one();
}
