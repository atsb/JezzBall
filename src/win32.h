#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "jezzball.h"

namespace win32 {

class Application {
public:
    explicit Application(HINSTANCE instance);
    int run();

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    void createWindow();
    void resizeForDpi(HWND hwnd);
    void updateLayout(HWND hwnd);
    void paint(HWND hwnd, HDC dc);
    void updateTimer();
    void resetRenderInterpolation();
    void captureRenderTransition(const std::array<jezzball::Ball, jezzball::kMaxBalls>& before);
    void captureSlicerTransition(const std::array<jezzball::Slicer, jezzball::kMaxSlicers>& before);
    void playSound(const wchar_t* name);

    void showLevelCompleteDialog(HWND hwnd);
    void showGameOverDialog(HWND hwnd);
    void showAbout(HWND hwnd);
    void showClassicMessage(HWND hwnd, int kind, const wchar_t* title, const wchar_t* text);
    bool showNewGameDialog(HWND hwnd);
    void showHighScores(HWND hwnd);
    bool showHighScoreNameDialog(HWND hwnd, int score, std::wstring& name);

    void handleCommand(HWND hwnd, UINT command);
    void updateMenus(HWND hwnd);

    void startDemo(HWND hwnd);
    void stopDemo(bool resetToGame);
    void tickDemo(HWND hwnd);
    bool chooseDemoCut(int& x, int& y, bool& vertical);

    void loadHighScores();
    void saveHighScores() const;
    bool qualifiesForHighScore(int score) const;
    void recordHighScore(int score, const std::wstring& name, int level);

    bool loadAssets();
    bool verifyEmbeddedResources() const;
    void unloadAssets();
    HBITMAP createBallBitmap(int sourceX, int sourceY) const;
    void drawAtlasTile(HDC dc, int dx, int dy, int dw, int dh, int sx, int sy, int sw, int sh, bool transparent) const;
    void drawAtlasQuarterTurn(HDC dc, int dx, int dy, int sx, int sy, int sw, int sh, int clockwise) const;
    void drawBackground(HDC dc);
    void drawHud(HDC dc);

    HINSTANCE m_instance = nullptr;
    HWND m_window = nullptr;
    UINT m_dpi = 96;
    HBITMAP m_atlas = nullptr;
    HDC m_atlasDC = nullptr;
    HGDIOBJ m_atlasOld = nullptr;
    std::array<HBITMAP, 22> m_ballBitmaps{};
    HBITMAP m_questionBitmap = nullptr;
    HBITMAP m_aboutLogoBitmap = nullptr;
    HBITMAP m_aboutIconBitmap = nullptr;
    HBITMAP m_highScoreTrophyBitmap = nullptr;
    HBITMAP m_highScoreIconBitmap = nullptr;
    HBITMAP m_messageBoxBitmap = nullptr;
    HBITMAP m_gameOverBaseBitmap = nullptr;
    HBITMAP m_newGameBaseBitmap = nullptr;
    HBITMAP m_highScoresBaseBitmap = nullptr;
    HBITMAP m_aboutBaseBitmap = nullptr;
    HBITMAP m_pausedBaseBitmap = nullptr;
    HDC m_ballDC = nullptr;
    int m_originX = 0;
    int m_originY = 0;
    int m_tileW = 16;
    int m_tileH = 12;
    RECT m_board{};

    bool m_soundEnabled = true;
    bool m_demoMode = false;
    int m_demoWait = 0;
    int m_demoAdvanceWait = 0;
    std::uint32_t m_demoRng = 0x6D2B79F5u;
    bool m_gameOverDialogShown = false;
    HWND m_pauseWindow = nullptr;
    struct RenderBallState {
        double x = 0.0;
        double y = 0.0;
        bool valid = false;
    };
    struct RenderSlicerState {
        double x = 0.0;
        double y = 0.0;
        bool valid = false;
    };
    std::array<RenderBallState, jezzball::kMaxBalls> m_renderFrom{};
    std::array<RenderBallState, jezzball::kMaxBalls> m_renderTo{};
    std::array<RenderSlicerState, jezzball::kMaxSlicers> m_renderSlicerFrom{};
    std::array<RenderSlicerState, jezzball::kMaxSlicers> m_renderSlicerTo{};
    ULONGLONG m_renderTransitionStart = 0;
    unsigned m_renderTransitionDuration = jezzball::kFastTickMs;

    std::vector<std::pair<std::wstring, int>> m_allHighScores;
    std::vector<std::pair<std::wstring, int>> m_todayHighScores;

    jezzball::Game m_game;
};

}
