// main_win.cpp - Windows front end: a window showing the 135x240 game screen
// scaled up, with the keyboard standing in for the stick's two buttons.
//   Space / Enter / Up  = front button      P / Tab / Down = side button
//   M = mute            Esc = quit
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include "../app/tower_app.h"

extern bool g_muted;   // sound_win.cpp

static Framebuffer g_fb;
static uint32_t    g_rgb32[Framebuffer::W * Framebuffer::H];
static int         g_scale = 3;
static bool        g_front = false, g_side = false, g_lostFocus = false, g_quit = false;
static HWND        g_hwnd = nullptr;

static const double FRAME_SECONDS = 1.0 / 30.0;   // 30 fps, the target for the stick

static void updateTitle() {
    SetWindowTextA(g_hwnd, g_muted
        ? "Tower of Babel   [Space] drop   [P] pause   [M] sound (off)   [Esc] quit"
        : "Tower of Babel   [Space] drop   [P] pause   [M] sound   [Esc] quit");
}

static void present(HDC dc) {
    // Convert RGB565 -> 32-bit for GDI, then scale up with hard pixel edges.
    for (int i = 0; i < Framebuffer::W * Framebuffer::H; ++i) {
        uint16_t c = g_fb.pixels[i];
        uint32_t r = ((c >> 11) & 31) * 255 / 31;
        uint32_t g = ((c >> 5) & 63) * 255 / 63;
        uint32_t b = (c & 31) * 255 / 31;
        g_rgb32[i] = (r << 16) | (g << 8) | b;
    }
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = Framebuffer::W;
    bmi.bmiHeader.biHeight = -Framebuffer::H;   // top-down rows
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc, 0, 0, Framebuffer::W * g_scale, Framebuffer::H * g_scale,
                  0, 0, Framebuffer::W, Framebuffer::H, g_rgb32, &bmi, DIB_RGB_COLORS, SRCCOPY);
}

static LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_KEYDOWN:
        if (l & (1 << 30)) return 0;   // ignore key auto-repeat
        switch (w) {
        case VK_SPACE: case VK_RETURN: case VK_UP: g_front = true; break;
        case 'P': case VK_TAB: case VK_DOWN:       g_side = true; break;
        case 'M': g_muted = !g_muted; updateTitle(); break;
        case VK_ESCAPE: DestroyWindow(h); break;
        }
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(w) == WA_INACTIVE) g_lostFocus = true;
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        present(dc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_DESTROY:
        g_quit = true;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, msg, w, l);
}

// ---- High score file, next to the .exe -------------------------------------
static void bestPath(char* out, DWORD size) {
    DWORD n = GetModuleFileNameA(nullptr, out, size);
    while (n > 0 && out[n - 1] != '\\') --n;
    out[n] = 0;
    strncat(out, "tower_best.txt", size - n - 1);
}

uint32_t platformLoadBest() {
    char path[MAX_PATH];
    bestPath(path, MAX_PATH);
    unsigned v = 0;
    if (FILE* f = fopen(path, "r")) {
        if (fscanf(f, "%u", &v) != 1) v = 0;
        fclose(f);
    }
    return v;
}

void platformSaveBest(uint32_t best) {
    char path[MAX_PATH];
    bestPath(path, MAX_PATH);
    if (FILE* f = fopen(path, "w")) {
        fprintf(f, "%u\n", (unsigned)best);
        fclose(f);
    }
}

// ---------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int show) {
    timeBeginPeriod(1);   // accurate Sleep() for steady frame timing

    // Pick the biggest whole-number scale that fits the screen (max 3x).
    RECT work;
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
    int fit = (work.bottom - work.top - 60) / Framebuffer::H;
    g_scale = fit < 1 ? 1 : (fit > 3 ? 3 : fit);

    WNDCLASSA wc = {};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.lpszClassName = "TowerOfBabel";
    RegisterClassA(&wc);

    DWORD style = WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;
    RECT r = { 0, 0, Framebuffer::W * g_scale, Framebuffer::H * g_scale };
    AdjustWindowRect(&r, style, FALSE);
    g_hwnd = CreateWindowA("TowerOfBabel", "Tower of Babel", style, CW_USEDEFAULT, CW_USEDEFAULT,
                           r.right - r.left, r.bottom - r.top, nullptr, nullptr, inst, nullptr);
    updateTitle();
    ShowWindow(g_hwnd, show);

    LARGE_INTEGER freq, prev, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);
    TowerApp app((uint32_t)prev.QuadPart, "SPACE", "P");

    while (!g_quit) {
        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) g_quit = true;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (g_quit) break;

        QueryPerformanceCounter(&now);
        double dt = (double)(now.QuadPart - prev.QuadPart) / (double)freq.QuadPart;
        if (dt < FRAME_SECONDS) { Sleep((DWORD)((FRAME_SECONDS - dt) * 1000.0)); continue; }
        prev = now;
        if (dt > 0.1) dt = 0.1;   // e.g. after dragging the window

        if (g_lostFocus) { app.pause(); g_lostFocus = false; }
        app.update((float)dt, g_front, g_side);
        g_front = g_side = false;
        app.render(g_fb);

        HDC dc = GetDC(g_hwnd);
        present(dc);
        ReleaseDC(g_hwnd, dc);
    }
    timeEndPeriod(1);
    return 0;
}
