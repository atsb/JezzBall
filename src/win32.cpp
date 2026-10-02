#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "win32.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <mmsystem.h>
#include <windowsx.h>

#include "../resources/resource.h"

#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "winmm.lib")

namespace {

constexpr wchar_t kClassName[] = L"JezzBallWin64Window";
constexpr wchar_t kWindowTitle[] = L"JezzBall";
constexpr UINT_PTR kTimerId = 1;
constexpr UINT_PTR kRenderTimerId = 2;
constexpr UINT kRenderIntervalMs = 16;
constexpr int kWindowWidth = 517;
constexpr int kWindowHeight = 374;
constexpr int kVisibleMinX = jezzball::kPlayableMinX;
constexpr int kVisibleMaxX = jezzball::kPlayableMaxX;
constexpr int kVisibleMinY = jezzball::kPlayableMinY;
constexpr int kVisibleMaxY = jezzball::kPlayableMaxY;
constexpr int kVisibleWidth = jezzball::kPlayableWidth;
constexpr int kVisibleHeight = jezzball::kPlayableHeight;
constexpr int kScreenTileW = 16;
constexpr int kScreenTileH = 12;
constexpr int kFloorSourceX = 0;
constexpr int kFloorSourceY = 0;
constexpr int kFloorSourceW = 16;
constexpr int kFloorSourceH = 12;
constexpr int kRedHorizontalWallBodySourceX = 200;
constexpr int kRedVerticalWallSourceX = 200;
constexpr int kBlueHorizontalWallBodySourceX = 232;
constexpr int kBlueVerticalWallSourceX = 232;
constexpr int kWallCapSourceX = 272;
constexpr int kRedWallCapSourceY = 0;
constexpr int kBlueWallCapSourceY = 12;
constexpr int kHorizontalWallSourceY = 6;
constexpr int kVerticalWallSourceY = 0;
constexpr int kHorizontalWallSourceW = 16;
constexpr int kHorizontalWallSourceH = 12;
constexpr int kWallCapSourceW = 16;
constexpr int kWallCapSourceH = 12;
constexpr int kVerticalWallSourceW = 8;
constexpr int kVerticalWallSourceH = 12;
constexpr int kHorizontalWallRenderW = 16;
constexpr int kHorizontalWallRenderH = 12;
constexpr int kVerticalWallRenderW = 12;
constexpr int kVerticalWallRenderH = 12;
static_assert(kRedHorizontalWallBodySourceX == 200 && kRedVerticalWallSourceX == 200, "Original red wall atlas coordinates changed");
static_assert(kBlueHorizontalWallBodySourceX == 232 && kBlueVerticalWallSourceX == 232, "Original blue wall atlas coordinates changed");
static_assert(kWallCapSourceX == 272 && kRedWallCapSourceY == 0 && kBlueWallCapSourceY == 12, "Original wall cap atlas coordinates changed");
static_assert(kHorizontalWallSourceY == 6 && kHorizontalWallSourceW == 16 && kHorizontalWallSourceH == 12, "Original horizontal wall atlas geometry changed");
static_assert(kWallCapSourceW == 16 && kWallCapSourceH == 12, "Original wall cap atlas geometry changed");
static_assert(kVerticalWallSourceY == 0 && kVerticalWallSourceW == 8 && kVerticalWallSourceH == 12, "Original vertical wall atlas geometry changed");
static_assert(kHorizontalWallRenderW == 16 && kHorizontalWallRenderH == 12, "Horizontal wall render geometry changed");
static_assert(kVerticalWallRenderW == 12 && kVerticalWallRenderH == 12, "Vertical wall render geometry changed");
constexpr int kBallFrameCount = 22;
constexpr int kBallFrameColumns = 11;
constexpr int kBallFirstFrameX = 16;
constexpr int kBallFrameSourceW = 16;
constexpr int kBallFrameSourceH = 12;

int maxInt(int a, int b)
{
    return a > b ? a : b;
}

RECT centerWindowRect(int width, int height)
{
    RECT rc{0, 0, width, height};
    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);

    const int workWidth = static_cast<int>(workArea.right - workArea.left);
    const int workHeight = static_cast<int>(workArea.bottom - workArea.top);
    const int x = static_cast<int>(workArea.left) + maxInt(0, (workWidth - width) / 2);
    const int y = static_cast<int>(workArea.top) + maxInt(0, (workHeight - height) / 2);

    OffsetRect(&rc, x, y);
    return rc;
}

struct LevelCompleteDialogContext {
    HWND owner = nullptr;
    int level = 1;
    HWND nextButton = nullptr;
};

constexpr wchar_t kLevelCompleteClass[] = L"JezzBallLevelCompleteDialog";
constexpr int kLevelCompleteWidth = 206;
constexpr int kLevelCompleteHeight = 165;
constexpr COLORREF kDialogBlue = RGB(0, 0, 195);
constexpr COLORREF kDialogGray = RGB(195, 195, 195);
constexpr COLORREF kDialogDarkGray = RGB(130, 130, 130);
constexpr int kPauseWidth = 88;
constexpr int kPauseHeight = 47;
constexpr wchar_t kPauseClass[] = L"JezzBallWin31Paused";
HBITMAP g_pausedBaseBitmap = nullptr;

void drawLevelCompleteButton(HDC dc, const RECT& rect, const wchar_t* text, HFONT font, bool pressed = false)
{
    RECT r = rect;
    HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
    HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH gray = CreateSolidBrush(kDialogGray);
    HBRUSH darkGray = CreateSolidBrush(kDialogDarkGray);

    FillRect(dc, &r, black);

    if (!pressed) {
        // Raised classic Windows 3.x button.
        RECT topLeft = {r.left + 1, r.top + 1, r.right - 1, r.bottom - 1};
        FillRect(dc, &topLeft, white);

        RECT bottomRight = {r.left + 2, r.top + 2, r.right - 1, r.bottom - 1};
        FillRect(dc, &bottomRight, darkGray);

        RECT face = {r.left + 3, r.top + 3, r.right - 2, r.bottom - 3};
        FillRect(dc, &face, gray);

        RECT textRect = {r.left + 2, r.top + 2, r.right - 2, r.bottom - 2};
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(0, 0, 0));
        SelectObject(dc, font);
        DrawTextW(dc, text, -1, &textRect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    else {
        // Sunken/pressed classic button.  The important visual cue is that
        // the dark bevel is on the top/left, the light bevel is on the
        // bottom/right, and the label is shifted one pixel down/right.
        RECT face = {r.left + 2, r.top + 2, r.right - 2, r.bottom - 2};
        FillRect(dc, &face, gray);

        RECT top = {r.left + 1, r.top + 1, r.right - 2, r.top + 2};
        RECT left = {r.left + 1, r.top + 1, r.left + 2, r.bottom - 2};
        RECT bottom = {r.left + 2, r.bottom - 2, r.right - 1, r.bottom - 1};
        RECT right = {r.right - 2, r.top + 2, r.right - 1, r.bottom - 1};
        FillRect(dc, &top, darkGray);
        FillRect(dc, &left, darkGray);
        FillRect(dc, &bottom, white);
        FillRect(dc, &right, white);

        RECT textRect = {r.left + 3, r.top + 3, r.right - 1, r.bottom - 1};
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(0, 0, 0));
        SelectObject(dc, font);
        DrawTextW(dc, text, -1, &textRect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    DeleteObject(black);
    DeleteObject(white);
    DeleteObject(gray);
    DeleteObject(darkGray);
}

void drawLevelCompleteSystemButton(HDC dc)
{
    RECT r = {5, 4, 23, 22};
    HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
    HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH gray = CreateSolidBrush(kDialogGray);
    HBRUSH darkGray = CreateSolidBrush(kDialogDarkGray);

    FillRect(dc, &r, black);

    RECT whiteRect = {r.left + 1, r.top + 1, r.right - 1, r.bottom - 1};
    FillRect(dc, &whiteRect, white);

    RECT face = {r.left + 2, r.top + 2, r.right - 2, r.bottom - 2};
    FillRect(dc, &face, gray);

    RECT darkBottom = {r.left + 2, r.bottom - 2, r.right - 1, r.bottom - 1};
    FillRect(dc, &darkBottom, darkGray);
    RECT darkRight = {r.right - 2, r.top + 2, r.right - 1, r.bottom - 2};
    FillRect(dc, &darkRight, darkGray);

    RECT minus = {r.left + 4, r.top + 8, r.right - 4, r.top + 10};
    FillRect(dc, &minus, black);

    DeleteObject(black);
    DeleteObject(white);
    DeleteObject(gray);
    DeleteObject(darkGray);
}

LRESULT CALLBACK levelCompleteDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* context = reinterpret_cast<LevelCompleteDialogContext*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* cs = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        context = static_cast<LevelCompleteDialogContext*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(context));
    }

    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);

        RECT client{};
        GetClientRect(hwnd, &client);
        HBRUSH blue = CreateSolidBrush(kDialogBlue);
        HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(dc, &client, blue);

        RECT body = {4, 23, kLevelCompleteWidth - 4, kLevelCompleteHeight - 4};
        FillRect(dc, &body, white);

        RECT titleBar = {4, 3, kLevelCompleteWidth - 4, 22};
        FillRect(dc, &titleBar, blue);

        RECT titleSeparator = {4, 22, kLevelCompleteWidth - 4, 23};
        FillRect(dc, &titleSeparator, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

        drawLevelCompleteSystemButton(dc);

        HFONT baseFont = static_cast<HFONT>(GetStockObject(SYSTEM_FONT));
        LOGFONTW lf{};
        GetObjectW(baseFont, sizeof(lf), &lf);
        lf.lfWeight = FW_BOLD;
        HFONT titleFont = CreateFontIndirectW(&lf);

        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        SelectObject(dc, titleFont);
        RECT titleRect = {28, 3, kLevelCompleteWidth - 4, 22};
        DrawTextW(dc, L"Well done!", -1, &titleRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SetTextColor(dc, RGB(0, 0, 0));
        SelectObject(dc, baseFont);

        RECT messageRect = {27, 39, 180, 97};
        FrameRect(dc, &messageRect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        InflateRect(&messageRect, -1, -1);
        FillRect(dc, &messageRect, white);

        const std::wstring text = L"You have completed\r\nLevel " + std::to_wstring(context ? context->level : 1);
        DrawTextW(dc, text.c_str(), -1, &messageRect, DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOPREFIX);

        // NEXT LEVEL is a real owner-draw Win32 button.  The native BUTTON
        // control owns the mouse capture/pressed state, which gives us the
        // classic depress-on-click behaviour reliably.

        DeleteObject(titleFont);
        DeleteObject(blue);
        DeleteObject(white);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DRAWITEM: {
        if (wParam == 1 && lParam != 0) {
            auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (draw->CtlType == ODT_BUTTON) {
                HFONT buttonFont = static_cast<HFONT>(GetStockObject(SYSTEM_FONT));
                const bool pressed = (draw->itemState & ODS_SELECTED) != 0;
                drawLevelCompleteButton(draw->hDC, draw->rcItem, L"NEXT LEVEL", buttonFont, pressed);
                return TRUE;
            }
        }
        break;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == 1 && HIWORD(wParam) == BN_CLICKED) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_KEYDOWN:
        if (wParam == VK_RETURN || wParam == VK_SPACE) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_LBUTTONDOWN:
        if (GET_Y_LPARAM(lParam) < 23 && GET_Y_LPARAM(lParam) >= 3) {
            ReleaseCapture();
            SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        }
        break;

    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        return TRUE;

    case WM_ERASEBKGND:
        return 1;

    case WM_CLOSE:
        return 0;

    case WM_DESTROY:
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}


std::filesystem::path executableDirectory();

enum class ClassicDialogKind {
    NewGame,
    GameOver,
    About,
    HelpContents,
    HelpHowToPlay,
    HelpCommands,
    HelpUseHelp,
    HighScores,
    EnterName,
    FileOpen,
    FileSaveAs,
    Generic
};

struct ClassicDialogContext {
    HWND owner = nullptr;
    ClassicDialogKind kind = ClassicDialogKind::Generic;
    int result = 0;
    int level = 1;
    int score = 0;
    int bonus = 0;
    HDC atlasDC = nullptr;
    std::vector<std::pair<std::wstring, int>> allScores;
    std::vector<std::pair<std::wstring, int>> todayScores;
    HWND edit = nullptr;
    std::wstring* nameOut = nullptr;
    HBITMAP questionBitmap = nullptr;
    HBITMAP aboutLogoBitmap = nullptr;
    HBITMAP aboutIconBitmap = nullptr;
    HBITMAP trophyBitmap = nullptr;
    HBITMAP highScoreIconBitmap = nullptr;
    HBITMAP messageBoxBitmap = nullptr;
    HBITMAP gameOverBaseBitmap = nullptr;
    HBITMAP newGameBaseBitmap = nullptr;
    HBITMAP highScoresBaseBitmap = nullptr;
    HBITMAP aboutBaseBitmap = nullptr;
    bool clearScoresRequested = false;
    int pressedButton = 0;
};

constexpr wchar_t kClassicDialogClass[] = L"JezzBallWin31Dialog";

RECT classicButtonRect(ClassicDialogKind kind, int which)
{
    if (kind == ClassicDialogKind::NewGame || kind == ClassicDialogKind::GameOver) {
        return which == 1 ? RECT{89, 97, 147, 125} : RECT{163, 97, 221, 125};
    }
    if (kind == ClassicDialogKind::HighScores) {
        return which == 1 ? RECT{151, 312, 218, 342} : RECT{226, 312, 335, 342};
    }
    if (kind == ClassicDialogKind::About) {
        return RECT{109, 236, 175, 266};
    }
    if (kind == ClassicDialogKind::EnterName) {
        return which == 1 ? RECT{42, 106, 112, 136} : RECT{120, 106, 190, 136};
    }
    if (kind == ClassicDialogKind::FileOpen || kind == ClassicDialogKind::FileSaveAs) {
        return which == 1 ? RECT{244, 154, 314, 184} : RECT{244, 190, 314, 220};
    }
    if (kind == ClassicDialogKind::HelpContents ||
        kind == ClassicDialogKind::HelpHowToPlay ||
        kind == ClassicDialogKind::HelpCommands ||
        kind == ClassicDialogKind::HelpUseHelp) {
        return RECT{80, 183, 180, 213};
    }
    return RECT{70, 154, 170, 184};
}

void drawClassicSystemButton(HDC dc, int width)
{
    RECT r{5, 4, 23, 22};
    HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
    HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH gray = CreateSolidBrush(kDialogGray);
    HBRUSH darkGray = CreateSolidBrush(kDialogDarkGray);
    FillRect(dc, &r, black);
    RECT a{r.left + 1, r.top + 1, r.right - 1, r.bottom - 1};
    FillRect(dc, &a, white);
    RECT b{r.left + 2, r.top + 2, r.right - 2, r.bottom - 2};
    FillRect(dc, &b, gray);
    RECT c{r.left + 2, r.bottom - 2, r.right - 1, r.bottom - 1};
    FillRect(dc, &c, darkGray);
    RECT d{r.right - 2, r.top + 2, r.right - 1, r.bottom - 2};
    FillRect(dc, &d, darkGray);
    RECT minus{r.left + 4, r.top + 8, r.right - 4, r.top + 10};
    FillRect(dc, &minus, black);
    DeleteObject(black);
    DeleteObject(white);
    DeleteObject(gray);
    DeleteObject(darkGray);
    (void)width;
}

void drawClassicButtonCommon(HDC dc, const RECT& rect, const wchar_t* text)
{
    HFONT font = static_cast<HFONT>(GetStockObject(SYSTEM_FONT));
    drawLevelCompleteButton(dc, rect, text, font);
}

void drawClassicButtonPressed(HDC dc, const RECT& rect, const wchar_t* text)
{
    HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
    HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH gray = CreateSolidBrush(kDialogGray);
    HBRUSH darkGray = CreateSolidBrush(kDialogDarkGray);

    FillRect(dc, &rect, black);
    RECT face{rect.left + 2, rect.top + 2, rect.right - 2, rect.bottom - 2};
    FillRect(dc, &face, gray);

    RECT top{rect.left + 1, rect.top + 1, rect.right - 2, rect.top + 2};
    RECT left{rect.left + 1, rect.top + 1, rect.left + 2, rect.bottom - 2};
    RECT bottom{rect.left + 2, rect.bottom - 2, rect.right - 1, rect.bottom - 1};
    RECT right{rect.right - 2, rect.top + 2, rect.right - 1, rect.bottom - 1};
    FillRect(dc, &top, darkGray);
    FillRect(dc, &left, darkGray);
    FillRect(dc, &bottom, white);
    FillRect(dc, &right, white);

    HFONT font = static_cast<HFONT>(GetStockObject(SYSTEM_FONT));
    RECT textRect = rect;
    OffsetRect(&textRect, 1, 1);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0, 0, 0));
    SelectObject(dc, font);
    DrawTextW(dc, text, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    DeleteObject(black);
    DeleteObject(white);
    DeleteObject(gray);
    DeleteObject(darkGray);
}

void drawClassicFrame(HDC dc, int width, int height, const wchar_t* title, bool grayBody, bool messageBoxStyle)
{
    HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
    HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH blue = CreateSolidBrush(kDialogBlue);

    RECT outer{0, 0, width, height};
    FillRect(dc, &outer, black);

    const int bodyTop = messageBoxStyle ? 27 : 23;
    const int titleBottom = messageBoxStyle ? 26 : 22;
    RECT body{4, bodyTop, width - 4, height - 4};
    HBRUSH bodyBrush = grayBody ? CreateSolidBrush(kDialogGray) : white;
    FillRect(dc, &body, bodyBrush);

    RECT titleBar{4, 3, width - 4, titleBottom};
    FillRect(dc, &titleBar, blue);

    RECT sep{4, titleBottom, width - 4, bodyTop};
    FillRect(dc, &sep, black);

    drawClassicSystemButton(dc, width);

    HFONT base = static_cast<HFONT>(GetStockObject(SYSTEM_FONT));
    LOGFONTW lf{};
    GetObjectW(base, sizeof(lf), &lf);
    lf.lfWeight = FW_BOLD;
    HFONT titleFont = CreateFontIndirectW(&lf);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    SelectObject(dc, titleFont);
    RECT titleRect{28, 3, width - 4, titleBottom};
    DrawTextW(dc, title, -1, &titleRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(dc, base);
    DeleteObject(titleFont);
    if (grayBody) {
        DeleteObject(bodyBrush);
    }
    DeleteObject(black);
    DeleteObject(white);
    DeleteObject(blue);
}

void drawClassicGroup(HDC dc, const RECT& rect, const wchar_t* label, HFONT font)
{
    SetTextColor(dc, RGB(0, 0, 0));
    SetBkMode(dc, TRANSPARENT);
    FrameRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    RECT textRect{rect.left + 8, rect.top - 5, rect.left + 100, rect.top + 8};
    FillRect(dc, &textRect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    SelectObject(dc, font);
    TextOutW(dc, rect.left + 12, rect.top - 5, label, static_cast<int>(wcslen(label)));
}

void drawAtlasDecoration(HDC dc, HDC atlasDC, int x, int y)
{
    if (!atlasDC) {
        return;
    }
    TransparentBlt(dc, x, y, 16, 12, atlasDC, kBallFirstFrameX, 0, 16, 12, RGB(0, 0, 0));
    TransparentBlt(dc, x + 20, y, 16, 12, atlasDC, (kBallFirstFrameX + 16), 0, 16, 12, RGB(0, 0, 0));
    TransparentBlt(dc, x + 40, y + 2, 16, 12, atlasDC, 8, 5, 12, 16, RGB(0, 0, 0));
}

void drawDialogBitmap(HDC dc, HBITMAP bitmap, int x, int y)
{
    if (!bitmap) {
        return;
    }
    BITMAP bm{};
    GetObjectW(bitmap, sizeof(bm), &bm);
    HDC src = CreateCompatibleDC(dc);
    if (!src) {
        return;
    }
    HGDIOBJ old = SelectObject(src, bitmap);
    BitBlt(dc, x, y, bm.bmWidth, bm.bmHeight, src, 0, 0, SRCCOPY);
    SelectObject(src, old);
    DeleteDC(src);
}

void drawPressedButtonOverlay(HDC dc, ClassicDialogContext* c)
{
    if (!c || c->pressedButton <= 0) {
        return;
    }

    const wchar_t* text = nullptr;
    switch (c->kind) {
    case ClassicDialogKind::NewGame:
    case ClassicDialogKind::GameOver:
        text = c->pressedButton == 1 ? L"Yes" : L"No";
        break;
    case ClassicDialogKind::HighScores:
        text = c->pressedButton == 1 ? L"OK" : L"Clear Scores";
        break;
    case ClassicDialogKind::About:
        text = L"OK";
        break;
    case ClassicDialogKind::EnterName:
        text = c->pressedButton == 1 ? L"OK" : L"CANCEL";
        break;
    case ClassicDialogKind::FileOpen:
    case ClassicDialogKind::FileSaveAs:
        text = c->pressedButton == 1 ? L"OK" : L"CANCEL";
        break;
    case ClassicDialogKind::HelpContents:
    case ClassicDialogKind::HelpHowToPlay:
    case ClassicDialogKind::HelpCommands:
    case ClassicDialogKind::HelpUseHelp:
        text = L"OK";
        break;
    default:
        break;
    }

    if (text) {
        drawClassicButtonPressed(dc, classicButtonRect(c->kind, c->pressedButton), text);
    }
}

void drawClassicDialogContent(HDC dc, ClassicDialogContext* c, int width, int height)
{
    SelectObject(dc, GetStockObject(SYSTEM_FONT));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0, 0, 0));

    switch (c->kind) {
    case ClassicDialogKind::NewGame:
    case ClassicDialogKind::GameOver: {
        HBITMAP messageBitmap = c->kind == ClassicDialogKind::GameOver ? c->gameOverBaseBitmap : c->newGameBaseBitmap;
        if (!messageBitmap) {
            messageBitmap = c->messageBoxBitmap;
        }
        if (messageBitmap) {
            drawDialogBitmap(dc, messageBitmap, 0, 0);
            if (c->kind == ClassicDialogKind::NewGame) {
                RECT title{28, 3, width - 4, 27};
                HBRUSH blue = CreateSolidBrush(kDialogBlue);
                FillRect(dc, &title, blue);
                DeleteObject(blue);
                SetTextColor(dc, RGB(255, 255, 255));
                DrawTextW(dc, L"JezzBall", -1, &title, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
        }
        else {
            drawClassicFrame(dc, width, height, c->kind == ClassicDialogKind::GameOver ? L"Game Over" : L"JezzBall", false, true);
            SetTextColor(dc, RGB(0, 0, 0));
            RECT textRect{68, 45, width - 18, 84};
            DrawTextW(dc, L"Do you want to start a new game?", -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            drawClassicButtonCommon(dc, classicButtonRect(c->kind, 1), L"Yes");
            drawClassicButtonCommon(dc, classicButtonRect(c->kind, 2), L"No");
        }
        break;
    }
    case ClassicDialogKind::About: {
        if (c->aboutBaseBitmap) {
            drawDialogBitmap(dc, c->aboutBaseBitmap, 0, 0);
            break;
        }
        if (c->aboutLogoBitmap) {
            drawDialogBitmap(dc, c->aboutLogoBitmap, 12, 32);
        }
        if (c->aboutIconBitmap) {
            drawDialogBitmap(dc, c->aboutIconBitmap, 20, 109);
        }
        RECT line1{78, 111, width - 8, 137};
        DrawTextW(dc, L"Jezz Ball", -1, &line1, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        RECT line2{20, 144, width - 20, 165};
        DrawTextW(dc, L"by Dima Pavlovsky", -1, &line2, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        RECT line3{18, 169, width - 18, 190};
        DrawTextW(dc, L"Produced by Marjacq Micro Ltd.", -1, &line3, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        RECT line4{10, 194, width - 10, 215};
        DrawTextW(dc, L"Copyright © 1992 Microsoft Corp.", -1, &line4, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        drawClassicButtonCommon(dc, classicButtonRect(c->kind, 1), L"OK");
        break;
    }
    case ClassicDialogKind::HelpContents:
    case ClassicDialogKind::HelpHowToPlay:
    case ClassicDialogKind::HelpCommands:
    case ClassicDialogKind::HelpUseHelp: {
        const wchar_t* heading = L"JezzBall Help";
        const wchar_t* body = L"";
        if (c->kind == ClassicDialogKind::HelpContents) {
            heading = L"Help Contents";
            body = L"How To Play\r\nCommands\r\nHow To Use Help";
        }
        else if (c->kind == ClassicDialogKind::HelpHowToPlay) {
            heading = L"How To Play";
            body = L"Point inside the chamber and click to build a wall.\r\n"
                   L"Right-click alternates horizontal and vertical walls.\r\n"
                   L"A wall grows until it reaches an edge or a ball.";
        }
        else if (c->kind == ClassicDialogKind::HelpCommands) {
            heading = L"Commands";
            body = L"F1  Help\r\nF2  New Game\r\nF3  Pause\r\n"
                   L"Right mouse button  Change wall direction";
        }
        else {
            heading = L"How To Use Help";
            body = L"Choose a topic from Help Contents.\r\n"
                   L"Use the scroll bar or links in a Windows Help viewer.";
        }
        RECT h{18, 31, width - 18, 51};
        DrawTextW(dc, heading, -1, &h, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Keep the help text clear of the button area. The help windows are
        // deliberately taller than the original placeholder so wrapped lines
        // are fully visible instead of being clipped at the bottom.
        RECT bodyRect{18, 56, width - 18, 176};
        DrawTextW(dc, body, -1, &bodyRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
        drawClassicButtonCommon(dc, classicButtonRect(c->kind, 1), L"OK");
        break;
    }
    case ClassicDialogKind::HighScores: {
        if (c->highScoresBaseBitmap) {
            drawDialogBitmap(dc, c->highScoresBaseBitmap, 0, 0);
        }
        else {
            const RECT topBox{12, 47, width - 13, 155};
            const RECT bottomBox{12, 186, width - 13, 295};
            FrameRect(dc, &topBox, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
            FrameRect(dc, &bottomBox, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
            HBRUSH groupFill = CreateSolidBrush(kDialogGray);
            RECT topLabel{62, 32, 282, 58};
            RECT bottomLabel{62, 171, 282, 197};
            FillRect(dc, &topLabel, groupFill);
            FillRect(dc, &bottomLabel, groupFill);
            DeleteObject(groupFill);
            FrameRect(dc, &topLabel, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
            FrameRect(dc, &bottomLabel, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
            drawClassicButtonCommon(dc, classicButtonRect(c->kind, 1), L"OK");
            drawClassicButtonCommon(dc, classicButtonRect(c->kind, 2), L"Clear Scores");
        }
        const int allY[] = {64, 91, 118};
        const int todayY[] = {205, 232, 259};
        for (int i = 0; i < 3; ++i) {
            wchar_t scoreText[32]{};
            if (i < static_cast<int>(c->allScores.size())) {
                _snwprintf_s(scoreText, _countof(scoreText), _TRUNCATE, L"%d", c->allScores[static_cast<std::size_t>(i)].second);
                TextOutW(dc, 69, allY[i], scoreText, static_cast<int>(wcslen(scoreText)));
            }
            if (i < static_cast<int>(c->todayScores.size())) {
                _snwprintf_s(scoreText, _countof(scoreText), _TRUNCATE, L"%d", c->todayScores[static_cast<std::size_t>(i)].second);
                TextOutW(dc, 69, todayY[i], scoreText, static_cast<int>(wcslen(scoreText)));
            }
        }
        break;
    }
    case ClassicDialogKind::EnterName: {
        RECT scoreRect{20, 34, width - 20, 55};
        wchar_t scoreText[64]{};
        _snwprintf_s(scoreText, _countof(scoreText), _TRUNCATE, L"Score: %d", c->score);
        DrawTextW(dc, scoreText, -1, &scoreRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        RECT prompt{20, 55, width - 20, 72};
        DrawTextW(dc, L"Enter your name:", -1, &prompt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        RECT editBox{20, 72, width - 20, 96};
        FrameRect(dc, &editBox, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        drawClassicButtonCommon(dc, classicButtonRect(c->kind, 1), L"OK");
        drawClassicButtonCommon(dc, classicButtonRect(c->kind, 2), L"CANCEL");
        break;
    }
    case ClassicDialogKind::FileOpen:
    case ClassicDialogKind::FileSaveAs: {
        const bool save = c->kind == ClassicDialogKind::FileSaveAs;
        TextOutW(dc, 12, 31, L"File name:", 10);
        RECT fileName{76, 28, width - 92, 50};
        FrameRect(dc, &fileName, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        TextOutW(dc, 12, 60, L"Directory:", 10);
        RECT dir{76, 57, width - 16, 79};
        FrameRect(dc, &dir, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        TextOutW(dc, 12, 89, L"Files:", 6);
        RECT files{12, 106, width / 2 - 10, 150};
        FrameRect(dc, &files, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        TextOutW(dc, width / 2 + 6, 89, L"Directories:", 12);
        RECT dirs{width / 2 + 6, 106, width - 90, 150};
        FrameRect(dc, &dirs, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        drawClassicButtonCommon(dc, classicButtonRect(c->kind, 1), L"OK");
        drawClassicButtonCommon(dc, classicButtonRect(c->kind, 2), L"CANCEL");
        if (save) {
            TextOutW(dc, 12, 155, L"File Save As", 11);
        }
        break;
    }
    default:
        drawClassicButtonCommon(dc, classicButtonRect(c->kind, 1), L"OK");
        break;
    }
}


void drawPausedWindow(HDC dc)
{
    if (g_pausedBaseBitmap) {
        drawDialogBitmap(dc, g_pausedBaseBitmap, 0, 0);
        return;
    }
    RECT client{0, 0, kPauseWidth, kPauseHeight};
    HBRUSH gray = CreateSolidBrush(kDialogGray);
    FillRect(dc, &client, gray);
    DeleteObject(gray);
    FrameRect(dc, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    RECT whiteTop{1, 1, kPauseWidth - 1, kPauseHeight - 1};
    FrameRect(dc, &whiteTop, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    RECT inner{2, 2, kPauseWidth - 2, kPauseHeight - 2};
    HBRUSH innerGray = CreateSolidBrush(kDialogGray);
    FillRect(dc, &inner, innerGray);
    DeleteObject(innerGray);
    HFONT font = static_cast<HFONT>(GetStockObject(SYSTEM_FONT));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0, 0, 0));
    SelectObject(dc, font);
    RECT text{4, 9, kPauseWidth - 4, 31};
    DrawTextW(dc, L"Paused", -1, &text, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

LRESULT CALLBACK pauseWindowProc(HWND hwnd, UINT message, WPARAM, LPARAM lParam)
{
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        drawPausedWindow(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        return TRUE;
    default:
        break;
    }
    (void)lParam;
    return DefWindowProcW(hwnd, message, 0, lParam);
}

LRESULT CALLBACK classicDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* c = reinterpret_cast<ClassicDialogContext*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* cs = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        c = static_cast<ClassicDialogContext*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(c));
    }

    switch (message) {
    case WM_CREATE:
        if (c && c->kind == ClassicDialogKind::EnterName) {
            c->edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                      WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                      21, 73, 178, 21, hwnd,
                                      reinterpret_cast<HMENU>(1001), GetModuleHandleW(nullptr), nullptr);
            if (c->edit) {
                SetFocus(c->edit);
            }
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT client{};
        GetClientRect(hwnd, &client);
        if (c && (c->kind == ClassicDialogKind::NewGame || c->kind == ClassicDialogKind::GameOver || c->kind == ClassicDialogKind::About || c->kind == ClassicDialogKind::HighScores)) {
            drawClassicDialogContent(dc, c, client.right, client.bottom);
            drawPressedButtonOverlay(dc, c);
        }
        else {
            drawClassicFrame(dc, client.right, client.bottom,
                             c && c->kind == ClassicDialogKind::EnterName ? L"High Score" :
                             c && c->kind == ClassicDialogKind::FileOpen ? L"File Open" :
                             c && c->kind == ClassicDialogKind::FileSaveAs ? L"File Save As" :
                             c && c->kind == ClassicDialogKind::HelpHowToPlay ? L"How To Play" :
                             c && c->kind == ClassicDialogKind::HelpCommands ? L"Commands" :
                             c && c->kind == ClassicDialogKind::HelpUseHelp ? L"How To Use Help" :
                             L"Help Contents", false, false);
            if (c) {
                drawClassicDialogContent(dc, c, client.right, client.bottom);
                drawPressedButtonOverlay(dc, c);
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == 1001 && HIWORD(wParam) == EN_CHANGE) {
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        break;
    case WM_LBUTTONDOWN: {
        if (!c) {
            break;
        }
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        int hit = 0;
        const RECT b1 = classicButtonRect(c->kind, 1);
        const RECT b2 = classicButtonRect(c->kind, 2);
        if (PtInRect(&b1, POINT{x, y})) {
            hit = 1;
        }
        else if (c->kind == ClassicDialogKind::HighScores ||
                 c->kind == ClassicDialogKind::NewGame ||
                 c->kind == ClassicDialogKind::GameOver ||
                 c->kind == ClassicDialogKind::EnterName ||
                 c->kind == ClassicDialogKind::FileOpen ||
                 c->kind == ClassicDialogKind::FileSaveAs) {
            if (PtInRect(&b2, POINT{x, y})) {
                hit = 2;
            }
        }
        if (hit != 0) {
            c->pressedButton = hit;
            SetCapture(hwnd);
            const RECT& repaint = hit == 1 ? b1 : b2;
            InvalidateRect(hwnd, &repaint, FALSE);
            UpdateWindow(hwnd);
            return 0;
        }
        if (y < 23 && y >= 3) {
            ReleaseCapture();
            SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        }
        break;
    }
    case WM_MOUSEMOVE:
        if (c && c->pressedButton != 0 && (wParam & MK_LBUTTON) != 0) {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            const int button = c->pressedButton > 0 ? c->pressedButton : -c->pressedButton;
            const RECT rect = classicButtonRect(c->kind, button);
            const bool inside = PtInRect(&rect, POINT{x, y}) != FALSE;
            c->pressedButton = inside ? button : -button;
            InvalidateRect(hwnd, &rect, FALSE);
            UpdateWindow(hwnd);
            return 0;
        }
        break;
    case WM_LBUTTONUP: {
        if (!c) {
            break;
        }
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        const int state = c->pressedButton;
        const int button = state > 0 ? state : -state;
        c->pressedButton = 0;
        ReleaseCapture();
        if (button == 0) {
            break;
        }
        const RECT rect = classicButtonRect(c->kind, button);
        const bool activate = state > 0 && PtInRect(&rect, POINT{x, y});
        InvalidateRect(hwnd, &rect, FALSE);
        UpdateWindow(hwnd);
        if (!activate) {
            return 0;
        }
        if (button == 2 && c->kind == ClassicDialogKind::HighScores) {
            c->allScores.clear();
            c->todayScores.clear();
            c->clearScoresRequested = true;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (button == 1) {
            if (c->kind == ClassicDialogKind::EnterName && c->edit && c->nameOut) {
                wchar_t name[64]{};
                GetWindowTextW(c->edit, name, _countof(name));
                *c->nameOut = name;
            }
            c->result = 1;
            DestroyWindow(hwnd);
            return 0;
        }
        if (button == 2) {
            c->result = 0;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            if (c) {
                c->result = 0;
            }
            DestroyWindow(hwnd);
            return 0;
        }
        if (wParam == VK_RETURN && c) {
            if (c->kind == ClassicDialogKind::EnterName) {
                if (c->edit && c->nameOut) {
                    wchar_t name[64]{};
                    GetWindowTextW(c->edit, name, _countof(name));
                    *c->nameOut = name;
                }
                c->result = 1;
                DestroyWindow(hwnd);
                return 0;
            }
            if (c->kind == ClassicDialogKind::NewGame || c->kind == ClassicDialogKind::GameOver) {
                c->result = 1;
                DestroyWindow(hwnd);
                return 0;
            }
            c->result = 1;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        return TRUE;
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        if (c) {
            c->result = 0;
        }
        DestroyWindow(hwnd);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool runClassicDialog(HINSTANCE instance, HWND owner, ClassicDialogContext& context, int width, int height, bool pauseOwner)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = &classicDialogProc;
    wc.lpszClassName = kClassicDialogClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetStockBrush(WHITE_BRUSH);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    static ATOM atom = 0;
    if (!atom) {
        atom = RegisterClassExW(&wc);
        if (!atom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }

    RECT ownerClient{};
    GetClientRect(owner, &ownerClient);
    POINT ownerScreen{0, 0};
    ClientToScreen(owner, &ownerScreen);
    const int ownerW = ownerClient.right;
    const int ownerH = ownerClient.bottom;
    const int x = ownerScreen.x + (ownerW - width) / 2;
    const int y = ownerScreen.y + (ownerH - height) / 2;

    const bool wasEnabled = IsWindowEnabled(owner) != FALSE;
    if (pauseOwner) {
        EnableWindow(owner, FALSE);
        KillTimer(owner, kTimerId);
        KillTimer(owner, kRenderTimerId);
    }

    HWND dialog = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT,
                                  kClassicDialogClass,
                                  L"",
                                  WS_POPUP,
                                  x, y, width, height,
                                  owner, nullptr, instance, &context);
    if (!dialog) {
        if (pauseOwner && wasEnabled) {
            EnableWindow(owner, TRUE);
        }
        return false;
    }

    ShowWindow(dialog, SW_SHOWNORMAL);
    UpdateWindow(dialog);
    SetForegroundWindow(dialog);
    SetFocus(dialog);

    MSG msg{};
    while (IsWindow(dialog) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (pauseOwner && wasEnabled) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
        SetFocus(owner);
    }
    return true;
}

std::wstring utf8ToWide(const std::string& text)
{
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
    return result;
}

std::string wideToUtf8(const std::wstring& text)
{
    if (text.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length, nullptr, nullptr);
    return result;
}


std::filesystem::path highScoreFilePath()
{
    wchar_t appData[1024]{};
    const DWORD length = GetEnvironmentVariableW(L"APPDATA", appData, _countof(appData));
    std::filesystem::path base;
    if (length != 0 && length < _countof(appData)) {
        base = std::filesystem::path(appData);
    }
    else {
        base = executableDirectory();
    }
    return base / L"JezzBall" / L"highscores.dat";
}

std::wstring currentDateString()
{
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t text[32]{};
    _snwprintf_s(text, _countof(text), _TRUNCATE, L"%04u-%02u-%02u", st.wYear, st.wMonth, st.wDay);
    return text;
}

std::filesystem::path executableDirectory()
{
    std::array<wchar_t, 1024> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }
    return std::filesystem::path(buffer.data(), buffer.data() + length).parent_path();
}

HBITMAP loadEmbeddedBitmapResource(HINSTANCE instance, int resourceId)
{
    return static_cast<HBITMAP>(LoadImageW(instance, MAKEINTRESOURCEW(resourceId), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
}

} 

namespace win32 {

Application::Application(HINSTANCE instance)
    : m_instance(instance)
{
    SetProcessDPIAware();
    loadHighScores();
}

int Application::run()
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = m_instance;
    wc.lpfnWndProc = &Application::windowProc;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(m_instance, MAKEINTRESOURCEW(IDI_APP));
    wc.hIconSm = wc.hIcon;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassExW(&wc)) {
        const DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            wchar_t message[256]{};
            _snwprintf_s(message, _countof(message), _TRUNCATE, L"JezzBall could not register window class. Windows error %lu.", error);
            MessageBoxW(nullptr, message, L"JezzBall", MB_OK | MB_ICONERROR);
            return 1;
        }
    }

    WNDCLASSEXW pwc{};
    pwc.cbSize = sizeof(pwc);
    pwc.hInstance = m_instance;
    pwc.lpfnWndProc = &pauseWindowProc;
    pwc.lpszClassName = kPauseClass;
    pwc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    pwc.hbrBackground = GetStockBrush(GRAY_BRUSH);
    static bool pauseRegistered = false;
    if (!pauseRegistered) {
        if (!RegisterClassExW(&pwc)) {
            const DWORD error = GetLastError();
            if (error != ERROR_CLASS_ALREADY_EXISTS) {
                return 1;
            }
        }
        pauseRegistered = true;
    }

    if (!loadAssets()) {
        MessageBoxW(nullptr,
                    L"JezzBall could not load graphics.",
                    L"JezzBall", MB_OK | MB_ICONERROR);
        return 2;
    }

    createWindow();
    if (!m_window) {
        MessageBoxW(nullptr,
                    L"JezzBall could not create main window.",
                    L"JezzBall", MB_OK | MB_ICONERROR);
        unloadAssets();
        return 3;
    }

    g_pausedBaseBitmap = m_pausedBaseBitmap;
    ShowWindow(m_window, SW_SHOWNORMAL);
    UpdateWindow(m_window);
    resetRenderInterpolation();

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    unloadAssets();
    return static_cast<int>(msg.wParam);
}

bool Application::verifyEmbeddedResources() const
{
    static const int bitmapIds[] = {
        IDB_ATLAS, IDB_GAMEOVER_QUESTION, IDB_ABOUT_LOGO, IDB_ABOUT_ICON,
        IDB_HIGHSCORE_TROPHY, IDB_HIGHSCORE_ICON, IDB_MESSAGEBOX_BASE,
        IDB_HIGHSCORES_BASE, IDB_ABOUT_BASE, IDB_PAUSED_BASE,
        IDB_GAMEOVER_BASE, IDB_NEWGAME_BASE
    };

    for (const int id : bitmapIds) {
        if (!FindResourceW(m_instance, MAKEINTRESOURCEW(id), RT_BITMAP)) {
            return false;
        }
    }

    return true;
}

bool Application::loadAssets()
{
    if (!verifyEmbeddedResources()) {
        MessageBoxW(nullptr,
                    L"JezzBall could not find resources.",
                    L"JezzBall", MB_OK | MB_ICONERROR);
        return false;
    }

    m_atlas = loadEmbeddedBitmapResource(m_instance, IDB_ATLAS);
    if (!m_atlas) {
        const DWORD error = GetLastError();
        wchar_t message[512]{};
        _snwprintf_s(message, _countof(message), _TRUNCATE,
                     L"JezzBall could not load graphics.\n\nWindows error %lu.",
                     error);
        MessageBoxW(nullptr, message, L"JezzBall", MB_OK | MB_ICONERROR);
        return false;
    }

    BITMAP atlasInfo{};
    if (GetObjectW(m_atlas, sizeof(atlasInfo), &atlasInfo) != sizeof(atlasInfo) ||
        atlasInfo.bmWidth != 288 || atlasInfo.bmHeight != 24) {
        MessageBoxW(nullptr,
                    L"JezzBall loaded an invalid graphics atlas.\n\nExpected: 288 x 24 pixels.",
                    L"JezzBall", MB_OK | MB_ICONERROR);
        unloadAssets();
        return false;
    }

    m_atlasDC = CreateCompatibleDC(nullptr);
    if (!m_atlasDC) {
        unloadAssets();
        return false;
    }

    m_atlasOld = SelectObject(m_atlasDC, m_atlas);
    if (!m_atlasOld) {
        unloadAssets();
        return false;
    }

    m_ballDC = CreateCompatibleDC(nullptr);
    if (!m_ballDC) {
        unloadAssets();
        return false;
    }

    for (int frame = 0; frame < kBallFrameCount; ++frame) {
        const int sourceX = kBallFirstFrameX + (frame % kBallFrameColumns) * kBallFrameSourceW;
        const int sourceY = (frame / kBallFrameColumns) * kBallFrameSourceH;
        m_ballBitmaps[static_cast<std::size_t>(frame)] = createBallBitmap(sourceX, sourceY);
    }
    m_questionBitmap = static_cast<HBITMAP>(loadEmbeddedBitmapResource(m_instance, IDB_GAMEOVER_QUESTION));
    m_aboutLogoBitmap = static_cast<HBITMAP>(loadEmbeddedBitmapResource(m_instance, IDB_ABOUT_LOGO));
    m_aboutIconBitmap = static_cast<HBITMAP>(loadEmbeddedBitmapResource(m_instance, IDB_ABOUT_ICON));
    m_highScoreTrophyBitmap = static_cast<HBITMAP>(loadEmbeddedBitmapResource(m_instance, IDB_HIGHSCORE_TROPHY));
    m_highScoreIconBitmap = static_cast<HBITMAP>(loadEmbeddedBitmapResource(m_instance, IDB_HIGHSCORE_ICON));
    m_messageBoxBitmap = static_cast<HBITMAP>(loadEmbeddedBitmapResource(m_instance, IDB_MESSAGEBOX_BASE));
    m_highScoresBaseBitmap = static_cast<HBITMAP>(loadEmbeddedBitmapResource(m_instance, IDB_HIGHSCORES_BASE));
    m_aboutBaseBitmap = static_cast<HBITMAP>(loadEmbeddedBitmapResource(m_instance, IDB_ABOUT_BASE));
    m_pausedBaseBitmap = static_cast<HBITMAP>(loadEmbeddedBitmapResource(m_instance, IDB_PAUSED_BASE));
    m_gameOverBaseBitmap = static_cast<HBITMAP>(loadEmbeddedBitmapResource(m_instance, IDB_GAMEOVER_BASE));
    m_newGameBaseBitmap = static_cast<HBITMAP>(loadEmbeddedBitmapResource(m_instance, IDB_NEWGAME_BASE));
    bool allBallFramesLoaded = true;
    for (int frame = 0; frame < kBallFrameCount; ++frame) {
        if (!m_ballBitmaps[static_cast<std::size_t>(frame)]) {
            allBallFramesLoaded = false;
            break;
        }
    }
    if (!allBallFramesLoaded) {
        MessageBoxW(nullptr,
                    L"JezzBall could not create all embedded atom animation frames.",
                    L"JezzBall", MB_OK | MB_ICONERROR);
        unloadAssets();
        return false;
    }

    const bool dialogsLoaded =
        m_questionBitmap != nullptr &&
        m_aboutLogoBitmap != nullptr &&
        m_aboutIconBitmap != nullptr &&
        m_highScoreTrophyBitmap != nullptr &&
        m_highScoreIconBitmap != nullptr &&
        m_messageBoxBitmap != nullptr &&
        m_highScoresBaseBitmap != nullptr &&
        m_aboutBaseBitmap != nullptr &&
        m_pausedBaseBitmap != nullptr &&
        m_gameOverBaseBitmap != nullptr &&
        m_newGameBaseBitmap != nullptr;

    if (!dialogsLoaded) {
        MessageBoxW(nullptr,
                    L"JezzBall could not load dialog artwork.",
                    L"JezzBall", MB_OK | MB_ICONERROR);
        unloadAssets();
        return false;
    }

    return m_atlas != nullptr && m_atlasDC != nullptr && m_ballDC != nullptr;
}

void Application::unloadAssets()
{
    if (m_ballDC) {
        DeleteDC(m_ballDC);
        m_ballDC = nullptr;
    }
    for (auto& bitmap : m_ballBitmaps) {
        if (bitmap) {
            DeleteObject(bitmap);
            bitmap = nullptr;
        }
    }
    if (m_questionBitmap) {
        DeleteObject(m_questionBitmap);
        m_questionBitmap = nullptr;
    }
    if (m_aboutLogoBitmap) {
        DeleteObject(m_aboutLogoBitmap);
        m_aboutLogoBitmap = nullptr;
    }
    if (m_aboutIconBitmap) {
        DeleteObject(m_aboutIconBitmap);
        m_aboutIconBitmap = nullptr;
    }
    if (m_highScoreTrophyBitmap) {
        DeleteObject(m_highScoreTrophyBitmap);
        m_highScoreTrophyBitmap = nullptr;
    }
    if (m_highScoreIconBitmap) {
        DeleteObject(m_highScoreIconBitmap);
        m_highScoreIconBitmap = nullptr;
    }
    if (m_messageBoxBitmap) {
        DeleteObject(m_messageBoxBitmap);
        m_messageBoxBitmap = nullptr;
    }
    if (m_highScoresBaseBitmap) {
        DeleteObject(m_highScoresBaseBitmap);
        m_highScoresBaseBitmap = nullptr;
    }
    if (m_aboutBaseBitmap) {
        DeleteObject(m_aboutBaseBitmap);
        m_aboutBaseBitmap = nullptr;
    }
    if (m_pausedBaseBitmap) {
        g_pausedBaseBitmap = nullptr;
        DeleteObject(m_pausedBaseBitmap);
        m_pausedBaseBitmap = nullptr;
    }
    if (m_gameOverBaseBitmap) {
        DeleteObject(m_gameOverBaseBitmap);
        m_gameOverBaseBitmap = nullptr;
    }
    if (m_newGameBaseBitmap) {
        DeleteObject(m_newGameBaseBitmap);
        m_newGameBaseBitmap = nullptr;
    }
    if (m_atlasDC) {
        if (m_atlasOld) {
            SelectObject(m_atlasDC, m_atlasOld);
            m_atlasOld = nullptr;
        }
        DeleteDC(m_atlasDC);
        m_atlasDC = nullptr;
    }
    if (m_atlas) {
        DeleteObject(m_atlas);
        m_atlas = nullptr;
    }
}


HBITMAP Application::createBallBitmap(int sourceX, int sourceY) const
{
    if (!m_atlasDC) {
        return nullptr;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = kScreenTileW;
    bmi.bmiHeader.biHeight = -kScreenTileH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap || !bits) {
        if (bitmap) {
            DeleteObject(bitmap);
        }
        return nullptr;
    }

    auto* pixels = static_cast<std::uint32_t*>(bits);
    std::vector<std::uint8_t> keep(static_cast<std::size_t>(kBallFrameSourceW * kBallFrameSourceH), 0);
    std::vector<std::uint32_t> sourcePixels(static_cast<std::size_t>(kBallFrameSourceW * kBallFrameSourceH), 0);

    for (int y = 0; y < kBallFrameSourceH; ++y) {
        for (int x = 0; x < kBallFrameSourceW; ++x) {
            const COLORREF color = GetPixel(m_atlasDC, sourceX + x, sourceY + y);
            if (color == CLR_INVALID) {
                continue;
            }

            const int r = GetRValue(color);
            const int g = GetGValue(color);
            const int b = GetBValue(color);
            const std::size_t index = static_cast<std::size_t>(y * kBallFrameSourceW + x);
            sourcePixels[index] =
                (static_cast<std::uint32_t>(r) << 16) |
                (static_cast<std::uint32_t>(g) << 8) |
                static_cast<std::uint32_t>(b);

            const bool red = r >= 160 && g <= 80 && b <= 80 && r > g + 60;
            const bool yellow = r >= 140 && g >= 60 && b <= 100 && r > b + 50 && g > b + 20;
            keep[index] = static_cast<std::uint8_t>((red || yellow) ? 1 : 0);
        }
    }

    for (int y = 0; y < kBallFrameSourceH; ++y) {
        for (int x = 0; x < kBallFrameSourceW; ++x) {
            const std::size_t index = static_cast<std::size_t>(y * kBallFrameSourceW + x);
            if (keep[index] != 0) {
                continue;
            }

            const std::uint32_t pixel = sourcePixels[index];
            const int r = static_cast<int>((pixel >> 16) & 0xFFu);
            const int g = static_cast<int>((pixel >> 8) & 0xFFu);
            const int b = static_cast<int>(pixel & 0xFFu);
            const bool darkRed = r >= 60 && r <= 190 && g <= 60 && b <= 60;
            const bool white = r >= 220 && g >= 220 && b >= 220;
            if (!darkRed && !white) {
                continue;
            }

            for (int oy = -1; oy <= 1 && keep[index] == 0; ++oy) {
                for (int ox = -1; ox <= 1; ++ox) {
                    const int nx = x + ox;
                    const int ny = y + oy;
                    if (nx < 0 || nx >= kBallFrameSourceW || ny < 0 || ny >= kBallFrameSourceH) {
                        continue;
                    }
                    if (keep[static_cast<std::size_t>(ny * kBallFrameSourceW + nx)] != 0) {
                        keep[index] = 1;
                        break;
                    }
                }
            }
        }
    }

    for (int y = 0; y < kScreenTileH; ++y) {
        for (int x = 0; x < kScreenTileW; ++x) {
            const std::size_t index = static_cast<std::size_t>(y * kScreenTileW + x);
            pixels[index] = keep[index] != 0
                ? 0xFF000000u | sourcePixels[index]
                : 0;
        }
    }

    return bitmap;
}

void Application::createWindow()
{
    m_dpi = 96;
    const RECT rc = centerWindowRect(kWindowWidth, kWindowHeight);

    HMENU menu = LoadMenuW(m_instance, MAKEINTRESOURCEW(IDM_GAME));

    m_window = CreateWindowExW(
        WS_EX_TOPMOST,
        kClassName,
        kWindowTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        rc.left,
        rc.top,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr,
        menu,
        m_instance,
        this);
}

void Application::resizeForDpi(HWND hwnd)
{
    SetWindowPos(hwnd,
                 nullptr,
                 0,
                 0,
                 kWindowWidth,
                 kWindowHeight,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Application::updateLayout(HWND hwnd)
{
    RECT client{};
    GetClientRect(hwnd, &client);

    m_tileW = kScreenTileW;
    m_tileH = kScreenTileH;
    const int boardWidth = m_tileW * kVisibleWidth;
    const int boardHeight = m_tileH * kVisibleHeight;
    m_originX = maxInt(0, (client.right - boardWidth) / 2);
    m_originY = maxInt(0, (client.bottom - boardHeight) / 2);
    m_board = {m_originX,
               m_originY,
               m_originX + boardWidth,
               m_originY + boardHeight};
}

void Application::playSound(const wchar_t* name)
{
    if (!m_soundEnabled || !name) {
        return;
    }

    const std::filesystem::path path = executableDirectory() / name;
    if (path.empty()) {
        return;
    }

    sndPlaySoundW(path.c_str(), SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}

void Application::showLevelCompleteDialog(HWND hwnd)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = m_instance;
    wc.lpfnWndProc = &levelCompleteDialogProc;
    wc.lpszClassName = kLevelCompleteClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetStockBrush(WHITE_BRUSH);
    static bool registered = false;
    if (!registered) {
        if (!RegisterClassExW(&wc)) {
            const DWORD error = GetLastError();
            if (error != ERROR_CLASS_ALREADY_EXISTS) {
                m_game.advanceToNextLevel();
                updateTimer();
                return;
            }
        }
        registered = true;
    }

    RECT client{};
    GetClientRect(hwnd, &client);
    POINT origin{client.left, client.top};
    ClientToScreen(hwnd, &origin);
    const int clientWidth = client.right - client.left;
    const int clientHeight = client.bottom - client.top;
    const int x = origin.x + (clientWidth - kLevelCompleteWidth) / 2;
    const int y = origin.y + (clientHeight - kLevelCompleteHeight) / 2;

    LevelCompleteDialogContext context{};
    context.owner = hwnd;
    context.level = m_game.completedLevel();

    InvalidateRect(hwnd, nullptr, FALSE);
    UpdateWindow(hwnd);
    EnableWindow(hwnd, FALSE);
    KillTimer(hwnd, kTimerId);
    KillTimer(hwnd, kRenderTimerId);
    playSound(L"JEZZDEAD.WAV");

    HWND dialog = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT,
                                  kLevelCompleteClass,
                                  L"Well done!",
                                  WS_POPUP,
                                  x, y, kLevelCompleteWidth, kLevelCompleteHeight,
                                  hwnd, nullptr, m_instance, &context);

    if (dialog) {
        context.nextButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"NEXT LEVEL",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            26, 111, 155, 40,
            dialog,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(1)),
            m_instance,
            nullptr);

        if (context.nextButton) {
            HFONT buttonFont = static_cast<HFONT>(GetStockObject(SYSTEM_FONT));
            SendMessageW(context.nextButton, WM_SETFONT, reinterpret_cast<WPARAM>(buttonFont), TRUE);
        }

        ShowWindow(dialog, SW_SHOWNORMAL);
        UpdateWindow(dialog);
        SetForegroundWindow(dialog);
        if (context.nextButton) {
            SetFocus(context.nextButton);
        }

        MSG msg{};
        while (IsWindow(dialog) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
            if (msg.message == WM_QUIT) {
                PostQuitMessage(static_cast<int>(msg.wParam));
                break;
            }
            if (msg.hwnd == dialog || IsChild(dialog, msg.hwnd)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            else {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
    }

    if (IsWindow(dialog)) {
        DestroyWindow(dialog);
    }

    EnableWindow(hwnd, TRUE);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    m_game.advanceToNextLevel();
    resetRenderInterpolation();
    updateTimer();
    InvalidateRect(hwnd, nullptr, FALSE);
}

void Application::showGameOverDialog(HWND hwnd)
{
    const int finalScore = m_game.score();
    const int finalLevel = m_game.level();

    loadHighScores();
    if (qualifiesForHighScore(finalScore)) {
        recordHighScore(finalScore, L"PLAYER", finalLevel);
    }

    ClassicDialogContext context{};
    context.owner = hwnd;
    context.kind = ClassicDialogKind::GameOver;
    context.score = finalScore;
    context.level = finalLevel;
    context.questionBitmap = m_questionBitmap;

    runClassicDialog(m_instance, hwnd, context, 313, 141, true);
    updateTimer();

    if (context.result == 1) {
        m_game.reset();
        resetRenderInterpolation();
        m_gameOverDialogShown = false;
        playSound(L"NEWBALL.WAV");
        updateTimer();
        updateMenus(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

void Application::updateTimer()
{
    if (m_window) {
        SetTimer(m_window, kTimerId, m_game.tickMilliseconds(), nullptr);
        SetTimer(m_window, kRenderTimerId, kRenderIntervalMs, nullptr);
    }
}

void Application::resetRenderInterpolation()
{
    for (int i = 0; i < jezzball::kMaxBalls; ++i) {
        const auto& ball = m_game.balls()[static_cast<std::size_t>(i)];
        m_renderFrom[static_cast<std::size_t>(i)].x = static_cast<double>(ball.x);
        m_renderFrom[static_cast<std::size_t>(i)].y = static_cast<double>(ball.y);
        m_renderFrom[static_cast<std::size_t>(i)].valid = ball.active;
        m_renderTo[static_cast<std::size_t>(i)] = m_renderFrom[static_cast<std::size_t>(i)];
    }

    for (int i = 0; i < jezzball::kMaxSlicers; ++i) {
        const auto& slicer = m_game.slicers()[static_cast<std::size_t>(i)];
        m_renderSlicerFrom[static_cast<std::size_t>(i)].x = static_cast<double>(slicer.x);
        m_renderSlicerFrom[static_cast<std::size_t>(i)].y = static_cast<double>(slicer.y);
        m_renderSlicerFrom[static_cast<std::size_t>(i)].valid = slicer.active;
        m_renderSlicerTo[static_cast<std::size_t>(i)] = m_renderSlicerFrom[static_cast<std::size_t>(i)];
    }

    m_renderTransitionStart = GetTickCount64();
    m_renderTransitionDuration = m_game.tickMilliseconds();
}

void Application::captureRenderTransition(const std::array<jezzball::Ball, jezzball::kMaxBalls>& before)
{
    bool changed = false;
    for (int i = 0; i < jezzball::kMaxBalls; ++i) {
        const auto& oldBall = before[static_cast<std::size_t>(i)];
        const auto& ball = m_game.balls()[static_cast<std::size_t>(i)];
        auto& from = m_renderFrom[static_cast<std::size_t>(i)];
        auto& to = m_renderTo[static_cast<std::size_t>(i)];
        if (!from.valid && !ball.active) {
            continue;
        }
        if (oldBall.active && ball.active &&
            (oldBall.x != ball.x || oldBall.y != ball.y)) {
            from.x = static_cast<double>(oldBall.x);
            from.y = static_cast<double>(oldBall.y);
            from.valid = true;
            to.x = static_cast<double>(ball.x);
            to.y = static_cast<double>(ball.y);
            to.valid = true;
            changed = true;
        }
        else if (!ball.active) {
            from.valid = false;
            to.valid = false;
        }
        else if (!from.valid) {
            from.x = static_cast<double>(ball.x);
            from.y = static_cast<double>(ball.y);
            from.valid = true;
            to = from;
        }
    }
    if (changed) {
        m_renderTransitionStart = GetTickCount64();
        m_renderTransitionDuration = m_game.tickMilliseconds();
    }
}

void Application::captureSlicerTransition(const std::array<jezzball::Slicer, jezzball::kMaxSlicers>& before)
{
    bool changed = false;

    for (int i = 0; i < jezzball::kMaxSlicers; ++i) {
        const auto& oldSlicer = before[static_cast<std::size_t>(i)];
        const auto& slicer = m_game.slicers()[static_cast<std::size_t>(i)];
        auto& from = m_renderSlicerFrom[static_cast<std::size_t>(i)];
        auto& to = m_renderSlicerTo[static_cast<std::size_t>(i)];

        if (oldSlicer.active && slicer.active &&
            (oldSlicer.x != slicer.x || oldSlicer.y != slicer.y)) {
            from.x = static_cast<double>(oldSlicer.x);
            from.y = static_cast<double>(oldSlicer.y);
            from.valid = true;
            to.x = static_cast<double>(slicer.x);
            to.y = static_cast<double>(slicer.y);
            to.valid = true;
            changed = true;
        }
        else if (!slicer.active) {
            from.valid = false;
            to.valid = false;
        }
        else if (!from.valid) {
            from.x = static_cast<double>(slicer.x);
            from.y = static_cast<double>(slicer.y);
            from.valid = true;
            to = from;
        }
    }

    if (changed) {
        m_renderTransitionStart = GetTickCount64();
        m_renderTransitionDuration = m_game.tickMilliseconds();
    }
}

void Application::updateMenus(HWND hwnd)
{
    HMENU menu = GetMenu(hwnd);
    if (!menu) {
        return;
    }

    CheckMenuItem(menu, IDM_OPTIONS_SLOW, MF_BYCOMMAND | (m_game.isFast() ? MF_UNCHECKED : MF_CHECKED));
    CheckMenuItem(menu, IDM_OPTIONS_FAST, MF_BYCOMMAND | (m_game.isFast() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, IDM_GAME_PAUSE, MF_BYCOMMAND | (m_game.isPaused() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, IDM_GAME_DEMO, MF_BYCOMMAND | (m_demoMode ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, IDM_OPTIONS_SOUND, MF_BYCOMMAND | (m_soundEnabled ? MF_CHECKED : MF_UNCHECKED));
    DrawMenuBar(hwnd);
}

void Application::handleCommand(HWND hwnd, UINT command)
{
    switch (command) {
    case IDM_GAME_NEW:
        if (m_demoMode) {
            stopDemo(false);
        }
        if (showNewGameDialog(hwnd)) {
            m_game.reset();
            resetRenderInterpolation();
            m_gameOverDialogShown = false;
            playSound(L"NEWBALL.WAV");
            updateTimer();
            updateMenus(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;
    case IDM_GAME_PAUSE:
        if (m_demoMode) {
            return;
        }
        m_game.togglePause();
        if (m_game.isPaused()) {
            RECT client{};
            GetClientRect(hwnd, &client);
            POINT origin{client.left, client.top};
            ClientToScreen(hwnd, &origin);
            const int x = origin.x + (client.right - kPauseWidth) / 2;
            const int y = origin.y + (client.bottom - kPauseHeight) / 2;
            if (!m_pauseWindow) {
                m_pauseWindow = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kPauseClass, L"Paused", WS_POPUP,
                                                x, y, kPauseWidth, kPauseHeight, hwnd, nullptr, m_instance, nullptr);
            }
            if (m_pauseWindow) {
                SetWindowPos(m_pauseWindow, HWND_TOP, x, y, kPauseWidth, kPauseHeight, SWP_NOACTIVATE | SWP_SHOWWINDOW);
                UpdateWindow(m_pauseWindow);
            }
        }
        else if (m_pauseWindow) {
            ShowWindow(m_pauseWindow, SW_HIDE);
        }
        updateMenus(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        break;
    case IDM_GAME_EXIT:
        DestroyWindow(hwnd);
        break;
    case IDM_OPTIONS_SLOW:
        m_game.setFast(false);
        updateTimer();
        updateMenus(hwnd);
        break;
    case IDM_OPTIONS_FAST:
        m_game.setFast(true);
        updateTimer();
        updateMenus(hwnd);
        break;
    case IDM_OPTIONS_SOUND:
        m_soundEnabled = !m_soundEnabled;
        if (!m_soundEnabled) {
            PlaySoundW(nullptr, nullptr, 0);
        }
        updateMenus(hwnd);
        break;
    case IDM_HELP_ABOUT:
        showAbout(hwnd);
        break;
    case IDM_HELP_CONTENTS:
        showClassicMessage(hwnd, static_cast<int>(ClassicDialogKind::HelpContents),
                           L"Help Contents",
                           L"Select a JezzBall help topic:\r\n\r\n"
                           L"How To Play\r\nCommands\r\nHow To Use Help");
        break;
    case IDM_HELP_HOWTOPLAY:
        showClassicMessage(hwnd, static_cast<int>(ClassicDialogKind::HelpHowToPlay),
                           L"How To Play", L"");
        break;
    case IDM_HELP_COMMANDS:
        showClassicMessage(hwnd, static_cast<int>(ClassicDialogKind::HelpCommands),
                           L"Commands", L"");
        break;
    case IDM_HELP_USEHELP:
        showClassicMessage(hwnd, static_cast<int>(ClassicDialogKind::HelpUseHelp),
                           L"How To Use Help", L"");
        break;
    case IDM_GAME_DEMO:
        if (m_demoMode) {
            stopDemo(true);
        }
        else {
            startDemo(hwnd);
        }
        break;
    case IDM_GAME_HIGHSCORES:
        showHighScores(hwnd);
        break;
    default:
        break;
    }
}

void Application::showClassicMessage(HWND hwnd, int kind, const wchar_t* title, const wchar_t* text)
{
    ClassicDialogContext context{};
    context.owner = hwnd;
    context.kind = static_cast<ClassicDialogKind>(kind);
    context.atlasDC = m_atlasDC;
    const bool helpDialog =
        context.kind == ClassicDialogKind::HelpContents ||
        context.kind == ClassicDialogKind::HelpHowToPlay ||
        context.kind == ClassicDialogKind::HelpCommands ||
        context.kind == ClassicDialogKind::HelpUseHelp;

    runClassicDialog(m_instance, hwnd, context,
                     context.kind == ClassicDialogKind::HighScores ? 420 : 260,
                     context.kind == ClassicDialogKind::HighScores ? 290 : (helpDialog ? 220 : 185),
                     true);
    updateTimer();
    (void)title;
    (void)text;
}

bool Application::showNewGameDialog(HWND hwnd)
{
    ClassicDialogContext context{};
    context.owner = hwnd;
    context.kind = ClassicDialogKind::NewGame;
    context.questionBitmap = m_questionBitmap;
    context.messageBoxBitmap = m_messageBoxBitmap;
    context.newGameBaseBitmap = m_newGameBaseBitmap;
    runClassicDialog(m_instance, hwnd, context, 313, 141, true);
    updateTimer();
    return context.result == 1;
}

void Application::showAbout(HWND hwnd)
{
    ClassicDialogContext context{};
    context.owner = hwnd;
    context.kind = ClassicDialogKind::About;
    context.aboutLogoBitmap = m_aboutLogoBitmap;
    context.aboutIconBitmap = m_aboutIconBitmap;
    context.aboutBaseBitmap = m_aboutBaseBitmap;
    runClassicDialog(m_instance, hwnd, context, 282, 279, true);
    updateTimer();
}

void Application::showHighScores(HWND hwnd)
{
    loadHighScores();
    ClassicDialogContext context{};
    context.owner = hwnd;
    context.kind = ClassicDialogKind::HighScores;
    context.allScores = m_allHighScores;
    context.todayScores = m_todayHighScores;
    context.trophyBitmap = m_highScoreTrophyBitmap;
    context.highScoreIconBitmap = m_highScoreIconBitmap;
    context.highScoresBaseBitmap = m_highScoresBaseBitmap;
    runClassicDialog(m_instance, hwnd, context, 344, 363, true);
    if (context.clearScoresRequested) {
        m_allHighScores.clear();
        m_todayHighScores.clear();
        saveHighScores();
    }
    updateTimer();
}

bool Application::showHighScoreNameDialog(HWND hwnd, int score, std::wstring& name)
{
    ClassicDialogContext context{};
    context.owner = hwnd;
    context.kind = ClassicDialogKind::EnterName;
    context.score = score;
    context.nameOut = &name;
    runClassicDialog(m_instance, hwnd, context, 220, 150, true);
    updateTimer();
    if (name.empty() && context.result == 1) {
        name = L"PLAYER";
    }
    return context.result == 1;
}

void Application::loadHighScores()
{
    m_allHighScores.clear();
    m_todayHighScores.clear();
    const std::filesystem::path path = highScoreFilePath();
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return;
    }

    const std::string today = wideToUtf8(currentDateString());
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream iss(line);
        std::string kind;
        std::string date;
        std::string scoreText;
        std::string nameText;
        if (!std::getline(iss, kind, '\t') ||
            !std::getline(iss, date, '\t') ||
            !std::getline(iss, scoreText, '\t') ||
            !std::getline(iss, nameText)) {
            continue;
        }

        try {
            const int score = std::stoi(scoreText);
            if (score < 0) {
                continue;
            }
            const std::wstring name = utf8ToWide(nameText);
            if (name.empty()) {
                continue;
            }
            if (kind == "H") {
                m_allHighScores.push_back({name, score});
            }
            if (kind == "T" && date == today) {
                m_todayHighScores.push_back({name, score});
            }
        }
        catch (...) {
        }
    }

    auto sortScores = [](auto& scores) {
        std::stable_sort(scores.begin(), scores.end(),
                         [](const auto& a, const auto& b) { return a.second > b.second; });
        if (scores.size() > 10) {
            scores.resize(10);
        }
    };
    sortScores(m_allHighScores);
    sortScores(m_todayHighScores);
}

void Application::saveHighScores() const
{
    const std::filesystem::path path = highScoreFilePath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        return;
    }

    const std::filesystem::path temp = path.wstring() + L".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return;
        }
        const std::string date = wideToUtf8(currentDateString());
        for (const auto& entry : m_allHighScores) {
            out << "H\t" << date << "\t" << entry.second << "\t" << wideToUtf8(entry.first) << "\n";
        }
        for (const auto& entry : m_todayHighScores) {
            out << "T\t" << date << "\t" << entry.second << "\t" << wideToUtf8(entry.first) << "\n";
        }
    }
    std::filesystem::remove(path, ec);
    ec.clear();
    std::filesystem::rename(temp, path, ec);
}

bool Application::qualifiesForHighScore(int score) const
{
    if (score <= 0) {
        return false;
    }
    if (m_allHighScores.size() < 10) {
        return true;
    }
    return score > m_allHighScores.back().second;
}

void Application::recordHighScore(int score, const std::wstring& name, int level)
{
    (void)level;
    if (score <= 0 || name.empty()) {
        return;
    }
    m_allHighScores.push_back({name, score});
    m_todayHighScores.push_back({name, score});

    auto normalize = [](auto& scores) {
        std::stable_sort(scores.begin(), scores.end(),
                         [](const auto& a, const auto& b) { return a.second > b.second; });
        if (scores.size() > 10) {
            scores.resize(10);
        }
    };
    normalize(m_allHighScores);
    normalize(m_todayHighScores);
    saveHighScores();
}

void Application::startDemo(HWND hwnd)
{
    m_demoMode = true;
    m_demoWait = 6;
    m_demoAdvanceWait = 0;
    m_demoRng ^= static_cast<std::uint32_t>(GetTickCount());
    m_game.setFast(true);
    m_game.reset();
    resetRenderInterpolation();
    m_gameOverDialogShown = false;
    updateTimer();
    updateMenus(hwnd);
    InvalidateRect(hwnd, nullptr, FALSE);
}

void Application::stopDemo(bool resetToGame)
{
    m_demoMode = false;
    m_demoWait = 0;
    m_demoAdvanceWait = 0;
    if (resetToGame) {
        m_game.reset();
        resetRenderInterpolation();
        m_gameOverDialogShown = false;
        if (m_window) {
            updateTimer();
            updateMenus(m_window);
            InvalidateRect(m_window, nullptr, FALSE);
        }
    }
}

bool Application::chooseDemoCut(int& x, int& y, bool& vertical)
{
    const auto& board = m_game.board();
    struct Candidate {
        int x;
        int y;
        bool vertical;
        int score;
    };
    Candidate best{0, 0, false, -1};

    for (int gy = jezzball::kPlayableMinY; gy <= jezzball::kPlayableMaxY; ++gy) {
        for (int gx = jezzball::kPlayableMinX; gx <= jezzball::kPlayableMaxX; ++gx) {
            const auto& cell = board[static_cast<std::size_t>((gy - 1) * jezzball::kBoardWidth + (gx - 1))];
            if (cell.state != static_cast<std::uint8_t>(jezzball::CellState::Open) || cell.ball != 0) {
                continue;
            }

            int horizontal = 0;
            for (int cx = gx; cx >= jezzball::kPlayableMinX; --cx) {
                const auto& c = board[static_cast<std::size_t>((gy - 1) * jezzball::kBoardWidth + (cx - 1))];
                if (c.state != static_cast<std::uint8_t>(jezzball::CellState::Open) || c.ball != 0) {
                    break;
                }
                ++horizontal;
            }
            for (int cx = gx + 1; cx <= jezzball::kPlayableMaxX; ++cx) {
                const auto& c = board[static_cast<std::size_t>((gy - 1) * jezzball::kBoardWidth + (cx - 1))];
                if (c.state != static_cast<std::uint8_t>(jezzball::CellState::Open) || c.ball != 0) {
                    break;
                }
                ++horizontal;
            }

            int vertical = 0;
            for (int cy = gy; cy >= jezzball::kPlayableMinY; --cy) {
                const auto& c = board[static_cast<std::size_t>((cy - 1) * jezzball::kBoardWidth + (gx - 1))];
                if (c.state != static_cast<std::uint8_t>(jezzball::CellState::Open) || c.ball != 0) {
                    break;
                }
                ++vertical;
            }
            for (int cy = gy + 1; cy <= jezzball::kPlayableMaxY; ++cy) {
                const auto& c = board[static_cast<std::size_t>((cy - 1) * jezzball::kBoardWidth + (gx - 1))];
                if (c.state != static_cast<std::uint8_t>(jezzball::CellState::Open) || c.ball != 0) {
                    break;
                }
                ++vertical;
            }

            const int centerPenalty = std::abs(gx - (jezzball::kPlayableMinX + jezzball::kPlayableMaxX) / 2)
                                     + std::abs(gy - (jezzball::kPlayableMinY + jezzball::kPlayableMaxY) / 2);
            const int hScore = horizontal * 12 - centerPenalty * 2 + static_cast<int>((m_demoRng >> 8) & 7u);
            const int vScore = vertical * 12 - centerPenalty * 2 + static_cast<int>((m_demoRng >> 11) & 7u);

            if (hScore > best.score) {
                best = {gx, gy, false, hScore};
            }
            if (vScore > best.score) {
                best = {gx, gy, true, vScore};
            }
        }
    }

    if (best.score < 0) {
        return false;
    }
    x = best.x;
    y = best.y;
    vertical = best.vertical;
    return true;
}

void Application::tickDemo(HWND hwnd)
{
    if (!m_demoMode) {
        return;
    }

    if (m_game.isLevelComplete()) {
        if (m_demoAdvanceWait++ >= 10) {
            playSound(L"NEWBALL.WAV");
            m_game.advanceToNextLevel();
            resetRenderInterpolation();
            m_demoAdvanceWait = 0;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return;
    }

    if (m_game.isGameOver()) {
        if (m_demoAdvanceWait++ >= 15) {
            m_game.reset();
            resetRenderInterpolation();
            m_demoAdvanceWait = 0;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return;
    }

    if (m_demoWait > 0) {
        --m_demoWait;
        return;
    }

    bool cutting = false;
    for (const auto& slicer : m_game.slicers()) {
        if (slicer.active) {
            cutting = true;
            break;
        }
    }
    if (cutting) {
        return;
    }

    int x = 0;
    int y = 0;
    bool vertical = false;
    if (chooseDemoCut(x, y, vertical)) {
        if (m_game.verticalMode() != vertical) {
            m_game.toggleOrientation();
        }
        m_game.beginCut(x, y);
        m_demoWait = 12;
    }
}

void Application::drawAtlasQuarterTurn(HDC dc, int dx, int dy,
                                          int sx, int sy, int sw, int sh, int clockwise) const
{
    if (!m_atlasDC || sw <= 0 || sh <= 0) {
        return;
    }

    const int dw = sh;
    const int dh = sw;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = dw;
    bmi.bmiHeader.biHeight = -dh;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap || !bits) {
        if (bitmap) {
            DeleteObject(bitmap);
        }
        return;
    }

    auto* pixels = static_cast<std::uint32_t*>(bits);
    for (int y = 0; y < dh; ++y) {
        for (int x = 0; x < dw; ++x) {
            const int srcX = clockwise != 0 ? y : sw - 1 - y;
            const int srcY = clockwise != 0 ? sh - 1 - x : x;
            const COLORREF color = GetPixel(m_atlasDC, sx + srcX, sy + srcY);
            if (color == CLR_INVALID) {
                continue;
            }
            pixels[static_cast<std::size_t>(y * dw + x)] =
                0xFF000000u |
                (static_cast<std::uint32_t>(GetRValue(color)) << 16) |
                (static_cast<std::uint32_t>(GetGValue(color)) << 8) |
                static_cast<std::uint32_t>(GetBValue(color));
        }
    }

    HDC dcBitmap = CreateCompatibleDC(dc);
    if (dcBitmap) {
        HGDIOBJ old = SelectObject(dcBitmap, bitmap);
        BitBlt(dc, dx, dy, dw, dh, dcBitmap, 0, 0, SRCCOPY);
        SelectObject(dcBitmap, old);
        DeleteDC(dcBitmap);
    }
    DeleteObject(bitmap);
}

void Application::drawAtlasTile(HDC dc, int dx, int dy, int dw, int dh,
                                int sx, int sy, int sw, int sh, bool transparent) const
{
    if (!m_atlasDC) {
        return;
    }

    if (transparent) {
        TransparentBlt(dc, dx, dy, dw, dh, m_atlasDC, sx, sy, sw, sh, RGB(0, 0, 0));
    }
    else if (dw == sw && dh == sh) {
        BitBlt(dc, dx, dy, dw, dh, m_atlasDC, sx, sy, SRCCOPY);
    }
    else {
        SetStretchBltMode(dc, COLORONCOLOR);
        StretchBlt(dc, dx, dy, dw, dh, m_atlasDC, sx, sy, sw, sh, SRCCOPY);
    }
}

void Application::drawBackground(HDC dc)
{
    FillRect(dc, &m_board, GetStockBrush(BLACK_BRUSH));
    SaveDC(dc);
    IntersectClipRect(dc, m_board.left, m_board.top, m_board.right, m_board.bottom);

    const auto& board = m_game.board();
    for (int y = kVisibleMinY; y <= kVisibleMaxY; ++y) {
        for (int x = kVisibleMinX; x <= kVisibleMaxX; ++x) {
            const int px = m_originX + (x - kVisibleMinX) * m_tileW;
            const int py = m_originY + (kVisibleMaxY - y) * m_tileH;
            const jezzball::Cell& c = board[static_cast<std::size_t>((y - 1) * jezzball::kBoardWidth + (x - 1))];
            const auto state = c.state;

            if (state == static_cast<std::uint8_t>(jezzball::CellState::Filled)) {
                continue;
            }

            drawAtlasTile(dc, px, py, kScreenTileW, kScreenTileH,
                          kFloorSourceX, kFloorSourceY,
                          kFloorSourceW, kFloorSourceH, false);

            if (state == static_cast<std::uint8_t>(jezzball::CellState::SliceA) ||
                state == static_cast<std::uint8_t>(jezzball::CellState::SliceB)) {
                const bool red = state == static_cast<std::uint8_t>(jezzball::CellState::SliceA);
                if (m_game.verticalMode()) {
                    const int sx = red ? kRedVerticalWallSourceX : kBlueVerticalWallSourceX;
                    const int dx = px + (kScreenTileW - kVerticalWallRenderW) / 2;
                    drawAtlasTile(dc, dx, py,
                                   kVerticalWallRenderW, kVerticalWallRenderH,
                                   sx, kVerticalWallSourceY,
                                   kVerticalWallSourceW, kVerticalWallSourceH, false);

                    const bool bottomEnd = y == kVisibleMinY ||
                        board[static_cast<std::size_t>((y - 2) * jezzball::kBoardWidth + (x - 1))].state != c.state;
                    const bool topEnd = y == kVisibleMaxY ||
                        board[static_cast<std::size_t>(y * jezzball::kBoardWidth + (x - 1))].state != c.state;

                    const int capSourceY = red ? kRedWallCapSourceY : kBlueWallCapSourceY;
                    if (bottomEnd) {
                        drawAtlasTile(dc, px, py,
                                       kScreenTileW, kScreenTileH,
                                       kWallCapSourceX, capSourceY,
                                       kWallCapSourceW, kWallCapSourceH, false);
                    }
                    if (topEnd) {
                        drawAtlasTile(dc, px, py,
                                       kScreenTileW, kScreenTileH,
                                       kWallCapSourceX, capSourceY,
                                       kWallCapSourceW, kWallCapSourceH, false);
                    }
                }
                else {
                    const int sx = red ? kRedHorizontalWallBodySourceX : kBlueHorizontalWallBodySourceX;
                    const int dy = py + (kScreenTileH - kHorizontalWallRenderH) / 2;
                    drawAtlasTile(dc, px, dy,
                                   kHorizontalWallRenderW, kHorizontalWallRenderH,
                                   sx, kHorizontalWallSourceY,
                                   kHorizontalWallSourceW, kHorizontalWallSourceH, false);

                    const bool leftEnd = x == kVisibleMinX ||
                        board[static_cast<std::size_t>((y - 1) * jezzball::kBoardWidth + (x - 2))].state != c.state;
                    const bool rightEnd = x == kVisibleMaxX ||
                        board[static_cast<std::size_t>((y - 1) * jezzball::kBoardWidth + x)].state != c.state;

                    const int capSourceY = red ? kRedWallCapSourceY : kBlueWallCapSourceY;
                    if (leftEnd) {
                        drawAtlasTile(dc, px, py,
                                       kScreenTileW, kScreenTileH,
                                       kWallCapSourceX, capSourceY,
                                       kWallCapSourceW, kWallCapSourceH, false);
                    }
                    if (rightEnd) {
                        drawAtlasTile(dc, px, py,
                                       kScreenTileW, kScreenTileH,
                                       kWallCapSourceX, capSourceY,
                                       kWallCapSourceW, kWallCapSourceH, false);
                    }
                }
            }
        }
    }

    const auto& slicers = m_game.slicers();
    const ULONGLONG renderNow = GetTickCount64();
    const double renderDuration = static_cast<double>(m_renderTransitionDuration == 0 ? 1u : m_renderTransitionDuration);
    const double renderT = std::min(1.0, static_cast<double>(renderNow >= m_renderTransitionStart ? renderNow - m_renderTransitionStart : 0) / renderDuration);

    for (int i = 0; i < jezzball::kMaxSlicers; ++i) {
        const auto& slicer = slicers[static_cast<std::size_t>(i)];
        if (!slicer.active) {
            continue;
        }

        auto from = m_renderSlicerFrom[static_cast<std::size_t>(i)];
        auto to = m_renderSlicerTo[static_cast<std::size_t>(i)];
        if (!from.valid || !to.valid) {
            from.x = to.x = static_cast<double>(slicer.x);
            from.y = to.y = static_cast<double>(slicer.y);
            from.valid = to.valid = true;
        }

        const double renderX = from.x + (to.x - from.x) * renderT;
        const double renderY = from.y + (to.y - from.y) * renderT;
        const int px = m_originX + static_cast<int>((renderX - static_cast<double>(kVisibleMinX)) * m_tileW);
        const int py = m_originY + static_cast<int>((static_cast<double>(kVisibleMaxY) - renderY) * m_tileH);

        if (m_game.verticalMode()) {
            const bool movingUp = slicer.dy > 0;
            const int capY = movingUp
                ? py
                : py + m_tileH - 2;
            RECT cap{px + (m_tileW - 8) / 2,
                     capY,
                     px + (m_tileW - 8) / 2 + 8,
                     capY + 2};
            FillRect(dc, &cap, GetStockBrush(WHITE_BRUSH));
        }
        else {
            const bool movingRight = slicer.dx > 0;
            const int capX = movingRight
                ? px + m_tileW - 2
                : px;
            RECT cap{capX,
                     py + (m_tileH - 8) / 2,
                     capX + 2,
                     py + (m_tileH - 8) / 2 + 8};
            FillRect(dc, &cap, GetStockBrush(WHITE_BRUSH));
        }
    }

    RestoreDC(dc, -1);

    SaveDC(dc);
    IntersectClipRect(dc, m_board.left, m_board.top, m_board.right, m_board.bottom);
    for (int i = 0; i < m_game.ballCount(); ++i) {
        const auto& ball = m_game.balls()[static_cast<std::size_t>(i)];
        if (!ball.active || !m_ballDC) {
            continue;
        }

        double visualX = static_cast<double>(ball.x);
        double visualY = static_cast<double>(ball.y);
        const auto& from = m_renderFrom[static_cast<std::size_t>(i)];
        const auto& to = m_renderTo[static_cast<std::size_t>(i)];
        if (from.valid && to.valid) {
            const ULONGLONG now = GetTickCount64();
            const ULONGLONG elapsed = now >= m_renderTransitionStart ? now - m_renderTransitionStart : 0;
            const double duration = static_cast<double>(m_renderTransitionDuration == 0 ? 1u : m_renderTransitionDuration);
            const double t = std::min(1.0, static_cast<double>(elapsed) / duration);
            visualX = from.x + (to.x - from.x) * t;
            visualY = from.y + (to.y - from.y) * t;
        }

        const int px = m_originX + static_cast<int>((visualX - static_cast<double>(kVisibleMinX)) * m_tileW);
        const int py = m_originY + static_cast<int>((static_cast<double>(kVisibleMaxY) - visualY) * m_tileH);
        const int frameIndex = static_cast<int>(ball.frame % kBallFrameCount);
        const HBITMAP selected = m_ballBitmaps[static_cast<std::size_t>(frameIndex)];
        if (selected) {
            HGDIOBJ old = SelectObject(m_ballDC, selected);
            BLENDFUNCTION blend{};
            blend.BlendOp = AC_SRC_OVER;
            blend.SourceConstantAlpha = 255;
            blend.AlphaFormat = AC_SRC_ALPHA;
            AlphaBlend(dc, px, py, m_tileW, m_tileH,
                       m_ballDC, 0, 0, kScreenTileW, kScreenTileH, blend);
            SelectObject(m_ballDC, old);
        }
    }
    RestoreDC(dc, -1);
}

void Application::drawHud(HDC dc)
{
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(dc, GetStockObject(SYSTEM_FONT));
    SetTextColor(dc, RGB(255, 255, 255));

    const std::wstring lives = L"Lives: " + std::to_wstring(m_game.lives());
    const std::wstring score = L"Score: " + std::to_wstring(static_cast<unsigned long long>(m_game.score()));
    const std::wstring bonus = L"Bonus: " + std::to_wstring(static_cast<unsigned long long>(m_game.levelBonus()));
    const std::wstring time = L"Time Left: " + std::to_wstring(m_game.timeRemaining());
    const std::wstring area = L"Area Cleared: " + std::to_wstring(m_game.areaClearedPercent()) + L"%";

    auto drawCentered = [&](const std::wstring& text, int centerX, int y) {
        SIZE size{};
        GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
        TextOutW(dc, centerX - size.cx / 2, y, text.c_str(), static_cast<int>(text.size()));
    };

    drawCentered(lives, 62, 10);
    drawCentered(score, 245, 2);
    if (m_game.isLevelComplete()) {
        drawCentered(bonus, 245, 20);
    }
    drawCentered(time, 415, 10);
    const int areaY = m_board.bottom + (m_board.top - 27);
    drawCentered(area, (m_board.left + m_board.right) / 2, areaY);

    SelectObject(dc, oldFont);
}

void Application::paint(HWND hwnd, HDC dc)
{
    RECT client{};
    GetClientRect(hwnd, &client);

    HDC mem = CreateCompatibleDC(dc);
    if (!mem) {
        return;
    }

    HBITMAP bmp = CreateCompatibleBitmap(dc, client.right, client.bottom);
    if (!bmp) {
        DeleteDC(mem);
        return;
    }

    HGDIOBJ oldBmp = SelectObject(mem, bmp);
    FillRect(mem, &client, GetStockBrush(BLACK_BRUSH));

    updateLayout(hwnd);

    drawBackground(mem);
    drawHud(mem);

    BitBlt(dc, 0, 0, client.right, client.bottom, mem, 0, 0, SRCCOPY);

    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
}

LRESULT CALLBACK Application::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Application* app = reinterpret_cast<Application*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* cs = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        app = static_cast<Application*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        app->m_window = hwnd;
    }

    if (app) {
        return app->handleMessage(hwnd, message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT Application::handleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE:
        m_dpi = 96;
        updateLayout(hwnd);
        updateTimer();
        updateMenus(hwnd);
        return 0;

    case WM_TIMER:
        if (wParam == kRenderTimerId) {
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (wParam == kTimerId) {
            if (m_demoMode) {
                tickDemo(hwnd);
            }
            std::array<std::pair<int, int>, jezzball::kMaxBalls> oldVelocity{};
            for (int i = 0; i < m_game.ballCount(); ++i) {
                const auto& ball = m_game.balls()[static_cast<std::size_t>(i)];
                oldVelocity[static_cast<std::size_t>(i)] = {ball.dx, ball.dy};
            }
            const int oldLives = m_game.lives();
            const int oldArea = m_game.areaClearedCells();
            const int oldTime = m_game.timeRemaining();
            const bool oldGameOver = m_game.isGameOver();
            const auto oldBalls = m_game.balls();
            const auto oldSlicers = m_game.slicers();

            m_game.tick();
            captureRenderTransition(oldBalls);
            captureSlicerTransition(oldSlicers);

            bool bounced = false;
            for (int i = 0; i < m_game.ballCount(); ++i) {
                const auto& ball = m_game.balls()[static_cast<std::size_t>(i)];
                if (oldVelocity[static_cast<std::size_t>(i)].first != ball.dx ||
                    oldVelocity[static_cast<std::size_t>(i)].second != ball.dy) {
                    bounced = true;
                    break;
                }
            }
            if (bounced) {
                playSound(L"BOUNCE.WAV");
            }
            if (m_game.lives() < oldLives) {
                playSound(L"JEZZDEAD.WAV");
            }
            if (m_game.areaClearedCells() > oldArea) {
                
            }
            if (m_game.isLevelComplete() && !m_demoMode) {
                showLevelCompleteDialog(hwnd);
            }
            if (!oldGameOver && m_game.isGameOver()) {
                
                if (!m_demoMode && !m_gameOverDialogShown) {
                    m_gameOverDialogShown = true;
                    showGameOverDialog(hwnd);
                }
            }
            if (oldTime > 200 && m_game.timeRemaining() <= 200) {
                
            }

        }
        return 0;

    case WM_COMMAND:
        handleCommand(hwnd, LOWORD(wParam));
        return 0;

    case WM_KEYDOWN:
        if (m_demoMode && wParam == VK_ESCAPE) {
            stopDemo(true);
            return 0;
        }
        if (wParam == VK_F2) {
            handleCommand(hwnd, IDM_GAME_NEW);
            return 0;
        }
        if (wParam == VK_F3) {
            handleCommand(hwnd, IDM_GAME_PAUSE);
            return 0;
        }
        if (wParam == VK_F1) {
            handleCommand(hwnd, IDM_HELP_CONTENTS);
            return 0;
        }
        break;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            POINT point{};
            GetCursorPos(&point);
            ScreenToClient(hwnd, &point);
            if (PtInRect(&m_board, point)) {
                SetCursor(LoadCursorW(nullptr, m_game.verticalMode() ? IDC_SIZENS : IDC_SIZEWE));
                return TRUE;
            }
        }
        break;

    case WM_RBUTTONUP:
        if (m_demoMode) {
            stopDemo(true);
            return 0;
        }
        m_game.toggleOrientation();
        {
            POINT point{};
            GetCursorPos(&point);
            ScreenToClient(hwnd, &point);
            if (PtInRect(&m_board, point)) {
                SetCursor(LoadCursorW(nullptr, m_game.verticalMode() ? IDC_SIZENS : IDC_SIZEWE));
            }
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONDOWN: {
        if (m_demoMode) {
            stopDemo(true);
            return 0;
        }
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        if (x >= m_board.left && x < m_board.right && y >= m_board.top && y < m_board.bottom) {
            const int gx = kVisibleMinX + ((x - m_board.left) / m_tileW);
            const int gy = kVisibleMaxY - ((y - m_board.top) / m_tileH);
            if (gx >= 1 && gx <= jezzball::kBoardWidth && gy >= 1 && gy <= jezzball::kBoardHeight) {
                const auto& cell = m_game.board()[static_cast<std::size_t>((gy - 1) * jezzball::kBoardWidth + (gx - 1))];
                if (!m_game.isPaused() && !m_game.isGameOver() &&
                    cell.state == static_cast<std::uint8_t>(jezzball::CellState::Open) && cell.ball == 0) {
                    
                    m_game.beginCut(gx, gy);
                }
            }
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_SIZE:
        updateLayout(hwnd);
        if (m_pauseWindow && m_game.isPaused()) {
            RECT client{};
            GetClientRect(hwnd, &client);
            POINT origin{client.left, client.top};
            ClientToScreen(hwnd, &origin);
            const int x = origin.x + (client.right - kPauseWidth) / 2;
            const int y = origin.y + (client.bottom - kPauseHeight) / 2;
            SetWindowPos(m_pauseWindow, HWND_TOP, x, y, kPauseWidth, kPauseHeight, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_DPICHANGED:
        m_dpi = 96;
        resizeForDpi(hwnd);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        paint(hwnd, dc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, kTimerId);
        KillTimer(hwnd, kRenderTimerId);
        if (m_pauseWindow) {
            DestroyWindow(m_pauseWindow);
            m_pauseWindow = nullptr;
        }
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}
