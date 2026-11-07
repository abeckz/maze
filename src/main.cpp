/**
 * maze.cpp -- Zufalls-Labyrinth mit Joystick-Steuerung auf dem ILI9488
 *
 * - Beim Start wird die Joystick-Ruhemitte automatisch kalibriert
 *   (Stick 2 s in Ruhe/Mitte lassen -- "Selbstkalibrierung").
 * - Danach wird per Recursive-Backtracker (randomisierte DFS) ein neues,
 *   garantiert loesbares Labyrinth erzeugt. Jeder Reset liefert ein anderes
 *   Labyrinth (Seed aus esp_random()).
 * - Der Spieler ist ein rotes Quadrat (Start unten links), das Ziel ein
 *   gruenes Quadrat (oben rechts).
 * - Steuerung zellweise: ein ueber die Schwelle gedrueckter Stick-Impuls
 *   bewegt den Spieler eine Zelle (Wand = blockiert). Erreicht der Spieler
 *   das Ziel, wird ein neues Labyrinth erzeugt.
 */
#include <Arduino.h>
#include <inttypes.h>
#include <esp_random.h>

#include "Selftest.hpp"
#include "Joystick.hpp"

using namespace selftest;

namespace {

// ---------------------------------------------------------------------------
// Labyrinth-Gitter. 320x480 in Rotation 0 (Hochformat). Oben bleibt eine
// Kopfzeile frei; das Labyrinth wird darunter zentriert.
// ---------------------------------------------------------------------------
constexpr int32_t MAZE_COLS   = 13;    // Zellen horizontal
constexpr int32_t MAZE_ROWS   = 19;    // Zellen vertikal

constexpr int32_t SCREEN_W    = 320;
constexpr int32_t SCREEN_H    = 480;

// Kopfzeile (Titel + Hinweis) oben, Labyrinth darunter.
constexpr int32_t HEADER_H    = 48;

// Zellgroesse ganzzahlig, damit es exakt rendert.
// 320/13 = 24.6 und (480-48)/19 = 22.7 -> 22 px, Labyrinth wird zentriert.
constexpr int32_t CELL        = 22;
constexpr int32_t MAZE_W      = MAZE_COLS * CELL;   // 13*22 = 286 px
constexpr int32_t MAZE_H      = MAZE_ROWS * CELL;   // 19*22 = 418 px

constexpr int32_t MAZE_X0     = (SCREEN_W - MAZE_W) / 2;                 // 17
constexpr int32_t MAZE_Y0     = HEADER_H + (SCREEN_H - HEADER_H - MAZE_H) / 2; // 55

// Zell-Wand-Bits (Recursive Backtracker entfernt Waende zwischen Nachbarn).
constexpr uint8_t WALL_N      = 0x01;
constexpr uint8_t WALL_S      = 0x02;
constexpr uint8_t WALL_W      = 0x04;
constexpr uint8_t WALL_E      = 0x08;
constexpr uint8_t WALL_ALL    = WALL_N | WALL_S | WALL_W | WALL_E;

// Spieler- und Ziel-Zellen.
struct Player
{
    int32_t col = 0;
    int32_t row = 0;
};

uint8_t walls[MAZE_ROWS][MAZE_COLS];   // pro Zelle die noch stehenden Waende
bool    visited[MAZE_ROWS][MAZE_COLS]; // temporaer fuer die Generierung

Player  player;
int32_t goalCol = MAZE_COLS - 1;
int32_t goalRow = MAZE_ROWS - 1;

// Joystick-Debounce: Ein Impuls = eine Zelle. Solange der Wert jenseits der
// Schwellen bleibt, wird erst nach Rueckkehr in die Mitte erneut ausgeloest.
bool    moveLatched[4] = { false, false, false, false };

// ---------------------------------------------------------------------------
// Zufall (xorshift32 -- kompakt und deterministisch pro Seed)
// ---------------------------------------------------------------------------
uint32_t rngState;

uint32_t rng()
{
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return rngState;
}

int32_t rngRange(int32_t n)
{
    if (n <= 0) return 0;
    return (int32_t)(rng() % (uint32_t)n);
}

// ---------------------------------------------------------------------------
// Zeichnen
// ---------------------------------------------------------------------------
uint32_t px(int32_t x) { return (uint32_t)(MAZE_X0 + x * CELL); }
uint32_t py(int32_t y) { return (uint32_t)(MAZE_Y0 + y * CELL); }

void drawGoal()
{
    const uint32_t x = px(goalCol);
    const uint32_t y = py(goalRow);
    const int32_t inset = 3;
    const uint32_t c = lcd.color888(0, 200, 0);
    lcd.fillRect(x + inset, y + inset, CELL - 2 * inset, CELL - 2 * inset, c);
}

void drawPlayer()
{
    const uint32_t x = px(player.col);
    const uint32_t y = py(player.row);
    const int32_t inset = 3;
    const uint32_t c = lcd.color888(220, 20, 20);
    lcd.fillRect(x + inset, y + inset, CELL - 2 * inset, CELL - 2 * inset, c);
}

// Loescht nur den Innenbereich einer Zelle und stellt ihre Waende wieder her.
void clearCell(int32_t col, int32_t row)
{
    const uint32_t x = px(col);
    const uint32_t y = py(row);
    lcd.fillRect(x + 1, y + 1, CELL - 2, CELL - 2, TFT_BLACK);
    const uint8_t w = walls[row][col];
    const uint32_t wallColor = lcd.color888(120, 120, 120);
    if (w & WALL_N) lcd.drawFastHLine(x, y, CELL, wallColor);
    if (w & WALL_S) lcd.drawFastHLine(x, y + CELL - 1, CELL, wallColor);
    if (w & WALL_W) lcd.drawFastVLine(x, y, CELL, wallColor);
    if (w & WALL_E) lcd.drawFastVLine(x + CELL - 1, y, CELL, wallColor);
}

void drawMaze()
{
    // Hintergrund des Labyrinth-Bereichs loeschen.
    lcd.fillRect(MAZE_X0 - 1, MAZE_Y0 - 1, MAZE_W + 2, MAZE_H + 2, TFT_BLACK);

    const uint32_t wallColor = lcd.color888(120, 120, 120);
    for (int32_t r = 0; r < MAZE_ROWS; ++r) {
        for (int32_t c = 0; c < MAZE_COLS; ++c) {
            const uint32_t x = px(c);
            const uint32_t y = py(r);
            const uint8_t w  = walls[r][c];
            if (w & WALL_N) lcd.drawFastHLine(x, y, CELL, wallColor);
            if (w & WALL_S) lcd.drawFastHLine(x, y + CELL - 1, CELL, wallColor);
            if (w & WALL_W) lcd.drawFastVLine(x, y, CELL, wallColor);
            if (w & WALL_E) lcd.drawFastVLine(x + CELL - 1, y, CELL, wallColor);
        }
    }

    drawGoal();
    drawPlayer();
}

void drawHeader(bool solved)
{
    lcd.fillRect(0, 0, SCREEN_W, HEADER_H, TFT_BLACK);
    lcd.setTextColor(TFT_CYAN, TFT_BLACK);
    lcd.setTextSize(2);
    lcd.setCursor(10, 4);
    lcd.print("Maze");

    lcd.setTextSize(1);
    lcd.setTextColor(TFT_GRAY, TFT_BLACK);
    lcd.setCursor(10, 30);
    if (solved) {
        lcd.setTextColor(TFT_GREEN, TFT_BLACK);
        lcd.print("Geschafft! Neues Labyrinth...");
    } else {
        lcd.print("Du=rot  Ziel=gruen");
    }
}

// ---------------------------------------------------------------------------
// Maze-Generierung (Recursive Backtracker / randomisierte DFS, iterativ)
// ---------------------------------------------------------------------------
void generateMaze()
{
    for (int32_t r = 0; r < MAZE_ROWS; ++r) {
        for (int32_t c = 0; c < MAZE_COLS; ++c) {
            walls[r][c]   = WALL_ALL;
            visited[r][c] = false;
        }
    }

    struct StackEntry { int32_t c, r; };
    static StackEntry stack[MAZE_ROWS * MAZE_COLS];
    int32_t top = 0;

    stack[top++] = { 0, 0 };
    visited[0][0] = true;

    while (top > 0) {
        StackEntry cur = stack[top - 1];

        int8_t  dx[4]   = { 0, 0, -1, 1 };
        int8_t  dy[4]   = { -1, 1, 0, 0 };
        uint8_t wbit[4] = { WALL_N, WALL_S, WALL_W, WALL_E };

        // Nachbarschafts-Reihenfolge zufaellig mischen (Fisher-Yates, 4 Elem).
        for (int i = 3; i > 0; --i) {
            int j = rngRange(i + 1);
            int8_t t;
            uint8_t tb;
            t = dx[i]; dx[i] = dx[j]; dx[j] = t;
            t = dy[i]; dy[i] = dy[j]; dy[j] = t;
            tb = wbit[i]; wbit[i] = wbit[j]; wbit[j] = tb;
        }

        bool advanced = false;
        for (int i = 0; i < 4; ++i) {
            int32_t nc = cur.c + dx[i];
            int32_t nr = cur.r + dy[i];
            if (nc < 0 || nc >= MAZE_COLS || nr < 0 || nr >= MAZE_ROWS) continue;
            if (visited[nr][nc]) continue;

            walls[cur.r][cur.c] &= ~wbit[i];
            uint8_t opp = 0;
            switch (wbit[i]) {
                case WALL_N: opp = WALL_S; break;
                case WALL_S: opp = WALL_N; break;
                case WALL_W: opp = WALL_E; break;
                case WALL_E: opp = WALL_W; break;
            }
            walls[nr][nc] &= ~opp;

            visited[nr][nc] = true;
            stack[top++] = { nc, nr };
            advanced = true;
            break;
        }

        if (!advanced) --top;
    }
}

// ---------------------------------------------------------------------------
// Spiel-Logik
// ---------------------------------------------------------------------------
void newGame()
{
    rngState = esp_random();
    generateMaze();

    player.col = 0;
    player.row = 0;

    goalCol = MAZE_COLS - 1;
    goalRow = MAZE_ROWS - 1;

    for (int i = 0; i < 4; ++i) moveLatched[i] = false;

    drawMaze();
    drawHeader(false);

    log("Neues Labyrinth: %" PRId32 "x%" PRId32 ", Seed 0x%08lX",
        MAZE_COLS, MAZE_ROWS, (unsigned long)rngState);
}

bool tryMove(int32_t dc, int32_t dr)
{
    int32_t nc = player.col + dc;
    int32_t nr = player.row + dr;
    if (nc < 0 || nc >= MAZE_COLS || nr < 0 || nr >= MAZE_ROWS) return false;

    uint8_t w = walls[player.row][player.col];
    if (dc ==  1 && (w & WALL_E)) return false;
    if (dc == -1 && (w & WALL_W)) return false;
    if (dr ==  1 && (w & WALL_S)) return false;
    if (dr == -1 && (w & WALL_N)) return false;

    clearCell(player.col, player.row);
    player.col = nc;
    player.row = nr;
    drawPlayer();

    if (player.col == goalCol && player.row == goalRow) {
        drawHeader(true);
        delay(1200);
        newGame();
    }
    return true;
}

// Lese Joystick, wandle normierte Werte in diskrete Richtungs-Impulse um.
void handleJoystick()
{
    joystick::Sample s = joystick::read();
    float nx = s.normX();
    float ny = s.normY();

    const float kThreshold = 0.55f;

    bool left  = nx < -kThreshold;
    bool right = nx >  kThreshold;
    bool up    = ny < -kThreshold;
    bool down  = ny >  kThreshold;

    // Latch: nur eine Flanke pro Haltephase -> ein Impuls = eine Zelle.
    if (left  && !moveLatched[0]) { moveLatched[0] = true; tryMove(-1, 0); }
    if (right && !moveLatched[1]) { moveLatched[1] = true; tryMove( 1, 0); }
    if (up    && !moveLatched[2]) { moveLatched[2] = true; tryMove( 0, -1); }
    if (down  && !moveLatched[3]) { moveLatched[3] = true; tryMove( 0,  1); }

    if (!left)  moveLatched[0] = false;
    if (!right) moveLatched[1] = false;
    if (!up)    moveLatched[2] = false;
    if (!down)  moveLatched[3] = false;
}

} // namespace

// ---------------------------------------------------------------------------
void setup()
{
    beginSerial("maze (Zufalls-Labyrinth + Joystick)");

    joystick::begin();
    log("Joystick-Pins: VX=GPIO%d, VY=GPIO%d", joystick::JOY_PIN_VX, joystick::JOY_PIN_VY);

    initDisplayOnce();
    if (!lcd.panel() || lcd.width() == 0) { log("=== ende setup ==="); return; }

    setNativeDepth();
    lcd.fillScreen(TFT_BLACK);

    // Selbstkalibrierung der Ruhemitte beim Start.
    lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    lcd.setTextSize(2);
    lcd.setCursor(10, 40);
    lcd.print("Kalibriere Joystick...");
    log("Kalibriere Joystick-Mitte ... Stick nicht bewegen!");
    joystick::calibrate([](const char* msg) { log("%s", msg); });

    newGame();
    log("=== Bereit. Steuere mit dem Joystick. ===");
}

void loop()
{
    handleJoystick();
    delay(16); // ~60 Hz Abtastung, ruhiges Bewegungsgefuehl
}

