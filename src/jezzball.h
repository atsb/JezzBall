#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace jezzball {

constexpr int kBoardWidth = 30;
constexpr int kBoardHeight = 22;
constexpr int kPlayableMinX = 2;
constexpr int kPlayableMaxX = kBoardWidth - 1;
constexpr int kPlayableMinY = 2;
constexpr int kPlayableMaxY = kBoardHeight - 1;
constexpr int kPlayableWidth = kPlayableMaxX - kPlayableMinX + 1;
constexpr int kPlayableHeight = kPlayableMaxY - kPlayableMinY + 1;
constexpr int kMaxBalls = 50;
constexpr int kMaxSlicers = 2;
constexpr unsigned kSlowTickMs = 100;
constexpr unsigned kFastTickMs = 50;
constexpr int kTargetClearPercent = 75;

struct Cell {
    std::uint8_t state = 1;
    std::uint8_t reserved = 0;
    std::uint8_t ball = 0;
    std::uint8_t visited = 0;
    std::uint8_t pad[2] = {0, 0};
};

enum class CellState : std::uint8_t {
    Filled = 0,
    Open = 1,
    SliceA = 2,
    SliceB = 3
};

struct Ball {
    int x = 1;
    int y = 1;
    int dx = 1;
    int dy = 1;
    int pendingX = 1;
    int pendingY = 1;
    bool hasPending = false;
    bool active = false;
    std::uint8_t frame = 0;
};

struct Slicer {
    bool active = false;
    int x = 0;
    int y = 0;
    int dx = 0;
    int dy = 0;
    int owner = 0;
    int pendingX = 0;
    int pendingY = 0;
    bool hasPending = false;
};

class Game {
public:
    Game();

    void reset();
    void tick();
    void togglePause();
    void setFast(bool fast);

    bool isPaused() const noexcept;
    bool isFast() const noexcept;
    bool isGameOver() const noexcept;
    bool isLevelComplete() const noexcept;
    unsigned tickMilliseconds() const noexcept;

    int level() const noexcept;
    int completedLevel() const noexcept;
    int levelBonus() const noexcept;
    int ballCount() const noexcept;
    int lives() const noexcept;
    int score() const noexcept;
    int areaClearedPercent() const noexcept;
    int areaClearedCells() const noexcept;
    int timeRemaining() const noexcept;

    bool verticalMode() const noexcept;
    void toggleOrientation();
    void beginCut(int x, int y);
    void advanceToNextLevel();

    const std::array<Cell, kBoardWidth * kBoardHeight>& board() const noexcept;
    const std::array<Ball, kMaxBalls>& balls() const noexcept;
    const std::array<Slicer, kMaxSlicers>& slicers() const noexcept;

private:
    static int indexOf(int x, int y) noexcept;
    static bool inside(int x, int y) noexcept;

    Cell& cell(int x, int y) noexcept;
    const Cell& cell(int x, int y) const noexcept;
    bool traversable(int x, int y) const noexcept;
    bool movementOpen(int x, int y) const noexcept;
    int findBallAt(int x, int y) const noexcept;

    void seedBalls();
    void clearBoard();
    void planBalls();
    void commitBalls();
    void advanceSlicers();
    void commitSlicers();
    void checkCutCompletion();
    void failSlicer(int index, bool loseLife);
    void completeSlicer(int index);
    void resolveBallSliceCollisions();
    void resolveBallSliceCollision(int index);
    int captureOpenRegions();
    void resetForNextLevel();
    void floodFillFromBalls(std::vector<std::uint8_t>& reachable);
    void updateTime();
    void addScoreForCells(int cellCount);
    int calculateLevelBonus() const noexcept;
    void recomputeArea();

    bool m_paused = false;
    bool m_fast = true;
    bool m_gameOver = false;
    bool m_levelComplete = false;
    bool m_verticalMode = false;
    bool m_cutHadFailure = false;
    bool m_cutInProgress = false;
    int m_level = 1;
    int m_ballCount = 2;
    int m_lives = 2;
    std::int64_t m_scoreHalfPoints = 0;
    int m_levelBonus = 0;
    int m_areaCleared = 0;
    int m_timeRemaining = 1500;
    int m_tickCounter = 0;
    unsigned m_timeAccumulatorMs = 0;
    std::uint32_t m_rng = 0x13579BDFu;
    std::array<Cell, kBoardWidth * kBoardHeight> m_board{};
    std::array<Ball, kMaxBalls> m_balls{};
    std::array<Slicer, kMaxSlicers> m_slicers{};
};

}
