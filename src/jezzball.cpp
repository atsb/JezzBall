#include "jezzball.h"

#include <algorithm>
#include <cstddef>
#include <queue>

namespace jezzball {

namespace {

std::uint32_t nextRandom(std::uint32_t& state)
{
    state = state * 1103515245u + 12345u;
    return state;
}

int randomRange(std::uint32_t& state, int low, int high)
{
    const std::uint32_t span = static_cast<std::uint32_t>(high - low + 1);
    return low + static_cast<int>((nextRandom(state) >> 8) % span);
}

int signFromRandom(std::uint32_t& state)
{
    return (nextRandom(state) & 1u) ? 1 : -1;
}

}

Game::Game()
{
    reset();
}

int Game::indexOf(int x, int y) noexcept
{
    return (y - 1) * kBoardWidth + (x - 1);
}

bool Game::inside(int x, int y) noexcept
{
    return x >= kPlayableMinX && x <= kPlayableMaxX &&
           y >= kPlayableMinY && y <= kPlayableMaxY;
}

Cell& Game::cell(int x, int y) noexcept
{
    return m_board[static_cast<std::size_t>(indexOf(x, y))];
}

const Cell& Game::cell(int x, int y) const noexcept
{
    return m_board[static_cast<std::size_t>(indexOf(x, y))];
}

bool Game::traversable(int x, int y) const noexcept
{
    if (!inside(x, y)) {
        return false;
    }
    return cell(x, y).state == static_cast<std::uint8_t>(CellState::Open);
}

bool Game::movementOpen(int x, int y) const noexcept
{
    return traversable(x, y) && cell(x, y).reserved == 0 && findBallAt(x, y) == 0;
}

int Game::findBallAt(int x, int y) const noexcept
{
    if (!inside(x, y)) {
        return 0;
    }
    return cell(x, y).ball;
}

void Game::clearBoard()
{
    for (auto& c : m_board) {
        c = {};
        c.state = static_cast<std::uint8_t>(CellState::Open);
    }
    m_areaCleared = 0;
}

void Game::seedBalls()
{
    for (auto& ball : m_balls) {
        ball = {};
    }

    for (int i = 0; i < m_ballCount; ++i) {
        Ball ball{};
        bool placed = false;
        for (int attempt = 0; attempt < 4096 && !placed; ++attempt) {
            ball.x = randomRange(m_rng, kPlayableMinX, kPlayableMaxX);
            ball.y = randomRange(m_rng, kPlayableMinY, kPlayableMaxY);
            if (findBallAt(ball.x, ball.y) == 0 && traversable(ball.x, ball.y)) {
                placed = true;
            }
        }
        if (!placed) {
            for (int y = kPlayableMinY; y <= kPlayableMaxY && !placed; ++y) {
                for (int x = kPlayableMinX; x <= kPlayableMaxX && !placed; ++x) {
                    if (traversable(x, y) && findBallAt(x, y) == 0) {
                        ball.x = x;
                        ball.y = y;
                        placed = true;
                    }
                }
            }
        }
        ball.dx = signFromRandom(m_rng);
        ball.dy = signFromRandom(m_rng);
        ball.pendingX = 0;
        ball.pendingY = 0;
        ball.hasPending = false;
        ball.active = true;
        ball.frame = 0;
        m_balls[static_cast<std::size_t>(i)] = ball;
        cell(ball.x, ball.y).ball = static_cast<std::uint8_t>(i + 1);
    }
}

void Game::reset()
{
    m_paused = false;
    m_gameOver = false;
    m_levelComplete = false;
    m_verticalMode = false;
    m_cutHadFailure = false;
    m_cutInProgress = false;
    m_level = 1;
    m_ballCount = 2;
    m_lives = m_ballCount;
    m_scoreHalfPoints = 0;
    m_levelBonus = 0;
    m_timeRemaining = m_ballCount * 250 + 1000;
    m_tickCounter = 0;
    m_timeAccumulatorMs = 0;
    m_slicers = {};
    clearBoard();
    seedBalls();
    recomputeArea();
}

void Game::togglePause()
{
    if (!m_gameOver) {
        m_paused = !m_paused;
    }
}

void Game::setFast(bool fast)
{
    m_fast = fast;
}

bool Game::isPaused() const noexcept
{
    return m_paused;
}

bool Game::isFast() const noexcept
{
    return m_fast;
}

bool Game::isGameOver() const noexcept
{
    return m_gameOver;
}

bool Game::isLevelComplete() const noexcept
{
    return m_levelComplete;
}

unsigned Game::tickMilliseconds() const noexcept
{
    return m_fast ? kFastTickMs : kSlowTickMs;
}

int Game::level() const noexcept
{
    return m_level;
}

int Game::completedLevel() const noexcept
{
    return m_level;
}

int Game::levelBonus() const noexcept
{
    return m_levelBonus;
}

int Game::ballCount() const noexcept
{
    return m_ballCount;
}

int Game::lives() const noexcept
{
    return m_lives;
}

int Game::score() const noexcept
{
    return static_cast<int>(m_scoreHalfPoints / 2);
}

int Game::areaClearedPercent() const noexcept
{
    return (m_areaCleared * 100) / (kPlayableWidth * kPlayableHeight);
}

int Game::areaClearedCells() const noexcept
{
    return m_areaCleared;
}

int Game::timeRemaining() const noexcept
{
    return m_timeRemaining;
}

bool Game::verticalMode() const noexcept
{
    return m_verticalMode;
}

void Game::toggleOrientation()
{
    if (!m_cutInProgress && !m_gameOver) {
        m_verticalMode = !m_verticalMode;
    }
}

void Game::beginCut(int x, int y)
{
    if (m_gameOver || m_paused || m_cutInProgress || !inside(x, y)) {
        return;
    }

    if (cell(x, y).state != static_cast<std::uint8_t>(CellState::Open) || findBallAt(x, y) != 0) {
        return;
    }

    m_slicers = {};
    m_cutHadFailure = false;
    m_cutInProgress = true;

    if (!m_verticalMode) {
        m_slicers[0] = {x > kPlayableMinX, x, y, -1, 0, 1, x, y, false};
        m_slicers[1] = {x < kPlayableMaxX, x + 1, y, 1, 0, 2, x + 1, y, false};
    }
    else {
        m_slicers[0] = {y < kPlayableMaxY, x, y, 0, 1, 1, x, y, false};
        m_slicers[1] = {y > kPlayableMinY, x, y - 1, 0, -1, 2, x, y - 1, false};
    }

    for (auto& slicer : m_slicers) {
        if (!slicer.active) {
            continue;
        }
        if (!movementOpen(slicer.x, slicer.y)) {
            slicer.active = false;
            continue;
        }
        cell(slicer.x, slicer.y).state = slicer.owner == 1
            ? static_cast<std::uint8_t>(CellState::SliceA)
            : static_cast<std::uint8_t>(CellState::SliceB);
    }

    if (!m_slicers[0].active && !m_slicers[1].active) {
        m_cutInProgress = false;
    }
}

void Game::failSlicer(int index, bool loseLife)
{
    if (index < 0 || index >= kMaxSlicers || !m_slicers[static_cast<std::size_t>(index)].active) {
        return;
    }

    const auto ownerState = index == 0
        ? static_cast<std::uint8_t>(CellState::SliceA)
        : static_cast<std::uint8_t>(CellState::SliceB);

    for (auto& c : m_board) {
        if (c.state == ownerState) {
            c.state = static_cast<std::uint8_t>(CellState::Open);
            c.reserved = 0;
        }
    }

    auto& slicer = m_slicers[static_cast<std::size_t>(index)];
    slicer.active = false;
    slicer.hasPending = false;
    slicer.pendingX = 0;
    slicer.pendingY = 0;
    m_cutHadFailure = true;

    if (loseLife && m_lives > 0) {
        --m_lives;
        if (m_lives <= 0) {
            m_gameOver = true;
            m_cutInProgress = false;
            m_slicers = {};
        }
    }
}

void Game::advanceSlicers()
{
    for (int i = 0; i < kMaxSlicers; ++i) {
        auto& slicer = m_slicers[static_cast<std::size_t>(i)];
        if (!slicer.active || slicer.hasPending) {
            continue;
        }

        const int nextX = slicer.x + slicer.dx;
        const int nextY = slicer.y + slicer.dy;

        if (!inside(nextX, nextY)) {
            completeSlicer(i);
            continue;
        }

        const auto& next = cell(nextX, nextY);
        if (next.state == static_cast<std::uint8_t>(CellState::Filled)) {
            completeSlicer(i);
            continue;
        }

        if (next.state != static_cast<std::uint8_t>(CellState::Open) ||
            next.reserved != 0 || next.ball != 0) {
            continue;
        }

        slicer.pendingX = nextX;
        slicer.pendingY = nextY;
        slicer.pendingX -= slicer.x;
        slicer.pendingY -= slicer.y;
        slicer.hasPending = true;

        cell(nextX, nextY).state = i == 0
            ? static_cast<std::uint8_t>(CellState::SliceA)
            : static_cast<std::uint8_t>(CellState::SliceB);
        cell(nextX, nextY).reserved = 1;
    }
}

void Game::commitSlicers()
{
    for (auto& slicer : m_slicers) {
        if (!slicer.active || !slicer.hasPending) {
            continue;
        }

        slicer.x += slicer.pendingX;
        slicer.y += slicer.pendingY;
        slicer.pendingX = 0;
        slicer.pendingY = 0;
        slicer.hasPending = false;
    }
}

void Game::planBalls()
{
    for (int i = 0; i < m_ballCount; ++i) {
        auto& ball = m_balls[static_cast<std::size_t>(i)];
        if (!ball.active) {
            continue;
        }

        ball.hasPending = false;

        const int x = ball.x;
        const int y = ball.y;
        const int dx = ball.dx;
        const int dy = ball.dy;

        if ((x <= kPlayableMinX && dx < 0) || (x >= kPlayableMaxX && dx > 0)) {
            ball.dx = -dx;
        }
        if ((y <= kPlayableMinY && dy < 0) || (y >= kPlayableMaxY && dy > 0)) {
            ball.dy = -dy;
        }

        const bool diagonalOpen = movementOpen(x + ball.dx, y + ball.dy);
        const bool verticalOpen = movementOpen(x, y + ball.dy);
        const bool horizontalOpen = movementOpen(x + ball.dx, y);

        if (!diagonalOpen || !verticalOpen || !horizontalOpen) {
            const int diagonalBall = findBallAt(x + dx, y + dy);
            if (diagonalBall > 0 && diagonalBall != i + 1) {
                m_balls[static_cast<std::size_t>(diagonalBall - 1)].dx = dx;
            }

            const int verticalBall = findBallAt(x, y + dy);
            if (verticalBall > 0 && verticalBall != i + 1) {
                m_balls[static_cast<std::size_t>(verticalBall - 1)].dy = dy;
            }

            const int horizontalBall = findBallAt(x + dx, y);
            if (horizontalBall > 0 && horizontalBall != i + 1) {
                m_balls[static_cast<std::size_t>(horizontalBall - 1)].dx = dx;
                m_balls[static_cast<std::size_t>(horizontalBall - 1)].dy = dy;
            }

            if (!diagonalOpen && verticalOpen) {
                ball.dx = -dx;
            }
            else if (!verticalOpen && diagonalOpen) {
                ball.dy = -dy;
            }
            else {
                ball.dx = -dx;
                ball.dy = -dy;
            }
        }

        const int moveX = ball.x + ball.dx;
        const int moveY = ball.y + ball.dy;
        const bool nextDiagonalOpen = movementOpen(moveX, moveY);
        const bool nextVerticalOpen = movementOpen(x, moveY);
        const bool nextHorizontalOpen = movementOpen(moveX, y);

        if (nextDiagonalOpen && nextVerticalOpen && nextHorizontalOpen) {
            ball.pendingX = ball.dx;
            ball.pendingY = ball.dy;
            ball.hasPending = true;
            ball.frame = static_cast<std::uint8_t>((ball.frame + 1u) % 22u);
            continue;
        }

        const int choices[4][2] = {
            {1, 1},
            {-1, -1},
            {1, -1},
            {-1, 1}
        };

        for (const auto& choice : choices) {
            const int candidateX = x + choice[0];
            const int candidateY = y + choice[1];
            if (movementOpen(candidateX, candidateY) &&
                movementOpen(candidateX, y) &&
                movementOpen(x, candidateY)) {
                ball.dx = choice[0];
                ball.dy = choice[1];
                ball.pendingX = ball.dx;
                ball.pendingY = ball.dy;
                ball.hasPending = true;
                ball.frame = static_cast<std::uint8_t>((ball.frame + 1u) % 22u);
                break;
            }
        }
    }
}

void Game::commitBalls()
{
    for (int i = 0; i < m_ballCount; ++i) {
        auto& ball = m_balls[static_cast<std::size_t>(i)];
        if (!ball.active || !ball.hasPending) {
            continue;
        }

        const int oldX = ball.x;
        const int oldY = ball.y;
        const int dx = ball.pendingX;
        const int dy = ball.pendingY;
        const int nextX = oldX + dx;
        const int nextY = oldY + dy;
        const int other = findBallAt(nextX, nextY);

        if (other > 0 && other != i + 1) {
            auto& otherBall = m_balls[static_cast<std::size_t>(other - 1)];
            otherBall.dx = dx;
            otherBall.dy = dy;
            ball.dx = -dx;
            ball.dy = -dy;
            ball.pendingX = 0;
            ball.pendingY = 0;
            ball.hasPending = false;
            continue;
        }

        if (inside(oldX, oldY) && cell(oldX, oldY).ball == static_cast<std::uint8_t>(i + 1)) {
            cell(oldX, oldY).ball = 0;
        }

        ball.x = nextX;
        ball.y = nextY;
        ball.pendingX = 0;
        ball.pendingY = 0;
        ball.hasPending = false;
        if (inside(ball.x, ball.y)) {
            cell(ball.x, ball.y).ball = static_cast<std::uint8_t>(i + 1);
        }

        resolveBallSliceCollision(i);
        if (m_gameOver) {
            return;
        }
    }
}

void Game::floodFillFromBalls(std::vector<std::uint8_t>& reachable)
{
    std::queue<std::pair<int, int>> queue;

    for (int i = 0; i < m_ballCount; ++i) {
        const auto& ball = m_balls[static_cast<std::size_t>(i)];
        if (ball.active && inside(ball.x, ball.y)) {
            const int idx = indexOf(ball.x, ball.y);
            if (reachable[static_cast<std::size_t>(idx)] == 0 && traversable(ball.x, ball.y)) {
                reachable[static_cast<std::size_t>(idx)] = 1;
                queue.push({ball.x, ball.y});
            }
        }
    }

    static constexpr int offsets[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    while (!queue.empty()) {
        const auto [x, y] = queue.front();
        queue.pop();

        for (const auto& offset : offsets) {
            const int nx = x + offset[0];
            const int ny = y + offset[1];
            if (!traversable(nx, ny)) {
                continue;
            }
            const int idx = indexOf(nx, ny);
            if (reachable[static_cast<std::size_t>(idx)] != 0) {
                continue;
            }
            reachable[static_cast<std::size_t>(idx)] = 1;
            queue.push({nx, ny});
        }
    }
}

void Game::completeSlicer(int index)
{
    if (index < 0 || index >= kMaxSlicers) {
        return;
    }

    auto& slicer = m_slicers[static_cast<std::size_t>(index)];
    if (!slicer.active) {
        return;
    }

    const auto ownerState = index == 0
        ? static_cast<std::uint8_t>(CellState::SliceA)
        : static_cast<std::uint8_t>(CellState::SliceB);

    int wallCells = 0;
    for (auto& c : m_board) {
        if (c.state == ownerState) {
            c.state = static_cast<std::uint8_t>(CellState::Filled);
            c.reserved = 0;
            ++wallCells;
        }
    }

    slicer.active = false;
    slicer.hasPending = false;
    slicer.pendingX = 0;
    slicer.pendingY = 0;

    const int capturedCells = captureOpenRegions();
    addScoreForCells(wallCells + capturedCells);
    recomputeArea();
}

void Game::resolveBallSliceCollision(int index)
{
    if (index < 0 || index >= m_ballCount) {
        return;
    }

    const auto& ball = m_balls[static_cast<std::size_t>(index)];
    if (!ball.active) {
        return;
    }

    static constexpr int offsets[8][2] = {
        {-1,-1}, {0,-1}, {1,-1},
        {-1, 0},          {1, 0},
        {-1, 1}, {0, 1}, {1, 1}
    };

    bool hitA = false;
    bool hitB = false;
    for (const auto& offset : offsets) {
        const int x = ball.x + offset[0];
        const int y = ball.y + offset[1];
        if (!inside(x, y)) {
            continue;
        }
        const auto state = cell(x, y).state;
        hitA = hitA || state == static_cast<std::uint8_t>(CellState::SliceA);
        hitB = hitB || state == static_cast<std::uint8_t>(CellState::SliceB);
    }

    if (hitA) {
        failSlicer(0, true);
    }
    if (hitB) {
        failSlicer(1, true);
    }
}

void Game::resolveBallSliceCollisions()
{
    for (int i = 0; i < m_ballCount; ++i) {
        resolveBallSliceCollision(i);
        if (m_gameOver) {
            return;
        }
    }
}

int Game::captureOpenRegions()
{
    std::array<std::uint8_t, kBoardWidth * kBoardHeight> visited{};
    int newlyFilled = 0;

    for (int y = 1; y <= kBoardHeight; ++y) {
        for (int x = 1; x <= kBoardWidth; ++x) {
            const int start = indexOf(x, y);
            if (visited[static_cast<std::size_t>(start)] != 0 || !traversable(x, y)) {
                continue;
            }

            std::queue<std::pair<int, int>> queue;
            std::vector<int> region;
            bool containsBall = false;
            visited[static_cast<std::size_t>(start)] = 1;
            queue.push({x, y});

            while (!queue.empty()) {
                const auto [cx, cy] = queue.front();
                queue.pop();
                const int idx = indexOf(cx, cy);
                region.push_back(idx);
                if (findBallAt(cx, cy) != 0) {
                    containsBall = true;
                }

                static constexpr int offsets[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
                for (const auto& offset : offsets) {
                    const int nx = cx + offset[0];
                    const int ny = cy + offset[1];
                    if (!traversable(nx, ny)) {
                        continue;
                    }
                    const int nidx = indexOf(nx, ny);
                    if (visited[static_cast<std::size_t>(nidx)] != 0) {
                        continue;
                    }
                    visited[static_cast<std::size_t>(nidx)] = 1;
                    queue.push({nx, ny});
                }
            }

            if (!containsBall) {
                for (const int idx : region) {
                    if (m_board[static_cast<std::size_t>(idx)].state == static_cast<std::uint8_t>(CellState::Open)) {
                        m_board[static_cast<std::size_t>(idx)].state = static_cast<std::uint8_t>(CellState::Filled);
                        ++newlyFilled;
                    }
                }
            }
        }
    }

    return newlyFilled;
}

void Game::checkCutCompletion()
{
    if (!m_cutInProgress) {
        return;
    }

    if (!m_slicers[0].active && !m_slicers[1].active) {
        m_cutInProgress = false;
        m_slicers = {};
        m_cutHadFailure = false;
        recomputeArea();
        if (areaClearedPercent() >= kTargetClearPercent) {
            m_levelBonus = calculateLevelBonus();
            m_scoreHalfPoints += static_cast<std::int64_t>(m_levelBonus) * 2;
            m_levelComplete = true;
        }
    }
}

void Game::recomputeArea()
{
    m_areaCleared = 0;
    for (const auto& c : m_board) {
        if (c.state == static_cast<std::uint8_t>(CellState::Filled)) {
            ++m_areaCleared;
        }
    }
}

void Game::resetForNextLevel()
{
    m_level++;
    m_ballCount = std::min(kMaxBalls, m_ballCount + 1);
    m_lives = m_ballCount;
    m_timeRemaining = m_ballCount * 250 + 1000;
    m_cutHadFailure = false;
    m_cutInProgress = false;
    m_slicers = {};
    clearBoard();
    seedBalls();
    recomputeArea();
}

int Game::calculateLevelBonus() const noexcept
{
    const int percent = areaClearedPercent();
    int bonus = m_lives * 50 + m_timeRemaining / 10;
    if (percent > 80) {
        bonus += (percent - 80) * (m_ballCount + 5) * 10;
    }
    if (percent > 90) {
        bonus += (percent - 90) * (m_ballCount + 5) * 20;
    }
    return bonus;
}

void Game::advanceToNextLevel()
{
    if (!m_levelComplete || m_gameOver) {
        return;
    }
    m_levelComplete = false;
    m_levelBonus = 0;
    resetForNextLevel();
}

void Game::addScoreForCells(int cellCount)
{
    if (cellCount <= 0) {
        return;
    }

    const std::int64_t baseHalfPoints = static_cast<std::int64_t>(m_ballCount + 5);
    const std::int64_t speedMultiplier = m_fast ? 2 : 1;
    m_scoreHalfPoints += static_cast<std::int64_t>(cellCount) * baseHalfPoints * speedMultiplier;
}

void Game::updateTime()
{
    if (m_timeRemaining > 0) {
        --m_timeRemaining;
    }
    if (m_timeRemaining <= 0) {
        m_timeRemaining = 0;
        m_gameOver = true;
        m_cutInProgress = false;
        m_slicers = {};
    }
}

void Game::tick()
{
    if (m_paused || m_gameOver || m_levelComplete) {
        return;
    }

    advanceSlicers();
    planBalls();
    commitSlicers();
    commitBalls();
    checkCutCompletion();

    m_timeAccumulatorMs += tickMilliseconds();
    while (m_timeAccumulatorMs >= 100u) {
        m_timeAccumulatorMs -= 100u;
        updateTime();
        if (m_gameOver) {
            break;
        }
    }

    ++m_tickCounter;
}

const std::array<Cell, kBoardWidth * kBoardHeight>& Game::board() const noexcept
{
    return m_board;
}

const std::array<Ball, kMaxBalls>& Game::balls() const noexcept
{
    return m_balls;
}

const std::array<Slicer, kMaxSlicers>& Game::slicers() const noexcept
{
    return m_slicers;
}

}
