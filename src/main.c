/*---------------------------------------------------------------------------------
    Pompom Tetris minigame -> SNES port

    Phase 2: grid + one controllable piece (d-pad move, L/Y+R/X rotate, A place).
    Phase 3: match detection + clearing + scoring.
    Phase 4a: real pieces -- the 10 shapes extracted from the Unity prefabs, each
      generated multi-color (2 of 5 colors per piece, per-cell). Replaces the
      placeholder square.
    Phase 4b: no self-clear -- a run of >=3 only clears if it connects the newly
      placed piece to same-color block(s) already on the board (creator's rule).
      A run wholly inside the piece just placed does NOT clear.

    Still text-console rendering (tile/sprite art = Phase 5): placed blocks =
    lowercase color letters, active piece = uppercase, '*' = active-over-placed.
    Wild/bomb + marble queue + spawn timer + game over come in 4c/4d.
---------------------------------------------------------------------------------*/
#include <snes.h>

#define GRID_W 9
#define GRID_H 12
#define GRID_X 11
#define GRID_Y 8
#define EMPTY 0
#define MAX_CELLS 4
#define GEN_COLORS 5 /* prefab totalColors: pieces use colors 0..4 */

// field[x][y]: 0 = empty, otherwise (color index + 1)
u8 field[GRID_W][GRID_H];
// marks the cells of the piece placed this turn (for the no-self-clear rule)
u8 justPlaced[GRID_W][GRID_H];

// Color glyphs (7 defined; generation uses the first GEN_COLORS)
const char PLACED_CH[7] = {'r', 'g', 'b', 'y', 'p', 'c', 'o'};
const char ACTIVE_CH[7] = {'R', 'G', 'B', 'Y', 'P', 'C', 'O'};

// Piece shapes (offsets), extracted from Assets/.../tetris/Game Objects/Block*.prefab
typedef struct { u8 n; s8 x[MAX_CELLS]; s8 y[MAX_CELLS]; } Shape;
const Shape SHAPES[] = {
    {1, {0},       {0}      }, // Single
    {2, {0, 0},    {0, 1}   }, // Double (domino)
    {3, {0, 0, 1}, {0, 1, 1}}, // Corner (L-tromino)
    {4, {0, 0, 0, 0}, {0, 1, 2, 3}}, // Vert (I, 4 tall)
    {4, {0, 0, 1, 1}, {0, 1, 0, 1}}, // Square (O)
    {4, {0, 1, 1, 2}, {1, 0, 1, 1}}, // T
    {4, {0, 1, 1, 1}, {0, 0, 1, 2}}, // L
    {4, {0, 0, 0, 1}, {0, 1, 2, 0}}, // Lx (J)
    {4, {0, 1, 1, 2}, {1, 0, 1, 0}}, // Z
    {4, {0, 1, 1, 2}, {0, 0, 1, 1}}, // Zx
};
#define NUM_SHAPES (sizeof(SHAPES) / sizeof(SHAPES[0]))

// Active piece state
u8 cellCount;
s8 curX[MAX_CELLS], curY[MAX_CELLS]; // current (rotated) offsets
u8 cellColor[MAX_CELLS];             // per-cell color 0..GEN_COLORS-1
int pieceX, pieceY;                  // anchor cell

u8 dirty;
u16 score;

//---------------------------------------------------------------------------------
u8 randn(u8 n) { return (u8)(rand() % n); }

u8 cellsInBounds(int ax, int ay, s8 *ox, s8 *oy)
{
    u8 i;
    for (i = 0; i < cellCount; i++)
    {
        int cx = ax + ox[i];
        int cy = ay + oy[i];
        if (cx < 0 || cx >= GRID_W || cy < 0 || cy >= GRID_H)
            return 0;
    }
    return 1;
}

u8 canPlace(void)
{
    u8 i;
    for (i = 0; i < cellCount; i++)
        if (field[pieceX + curX[i]][pieceY + curY[i]] != EMPTY)
            return 0;
    return 1;
}

// SNES crt0 does not zero-initialize globals, so clear these explicitly.
void clearBoard(void)
{
    u8 x, y;
    for (x = 0; x < GRID_W; x++)
        for (y = 0; y < GRID_H; y++)
        {
            field[x][y] = EMPTY;
            justPlaced[x][y] = 0;
        }
}

// Generate a new random piece (random shape, 2 random colors) at top-center.
void spawnPiece(void)
{
    u8 i, s, c0, c1;
    s8 maxx = 0;

    s = randn(NUM_SHAPES);
    cellCount = SHAPES[s].n;
    for (i = 0; i < cellCount; i++)
    {
        curX[i] = SHAPES[s].x[i];
        curY[i] = SHAPES[s].y[i];
        if (curX[i] > maxx) maxx = curX[i];
    }

    // Pick 2 colors; each cell is one of the two (per TetrisBlockGroup.ChooseBricks)
    c0 = randn(GEN_COLORS);
    c1 = randn(GEN_COLORS);
    for (i = 0; i < cellCount; i++)
        cellColor[i] = (rand() & 1) ? c1 : c0;

    pieceX = (GRID_W - 1 - maxx) / 2;
    pieceY = 0;
    dirty = 1;
}

void tryMove(int dx, int dy)
{
    if (cellsInBounds(pieceX + dx, pieceY + dy, curX, curY))
    {
        pieceX += dx;
        pieceY += dy;
        dirty = 1;
    }
}

// dir > 0 = clockwise, dir < 0 = counter-clockwise, with a simple wall kick.
void tryRotate(int dir)
{
    s8 rx[MAX_CELLS], ry[MAX_CELLS];
    int minx = 127, maxx = -128, miny = 127, maxy = -128;
    int shiftX = 0, shiftY = 0;
    u8 i;

    for (i = 0; i < cellCount; i++)
    {
        if (dir > 0) { rx[i] = curY[i];  ry[i] = -curX[i]; }
        else         { rx[i] = -curY[i]; ry[i] = curX[i];  }
    }

    for (i = 0; i < cellCount; i++)
    {
        int cx = pieceX + rx[i];
        int cy = pieceY + ry[i];
        if (cx < minx) minx = cx;
        if (cx > maxx) maxx = cx;
        if (cy < miny) miny = cy;
        if (cy > maxy) maxy = cy;
    }
    if (minx < 0) shiftX = -minx;
    else if (maxx > GRID_W - 1) shiftX = (GRID_W - 1) - maxx;
    if (miny < 0) shiftY = -miny;
    else if (maxy > GRID_H - 1) shiftY = (GRID_H - 1) - maxy;

    if (!cellsInBounds(pieceX + shiftX, pieceY + shiftY, rx, ry))
        return;

    for (i = 0; i < cellCount; i++) { curX[i] = rx[i]; curY[i] = ry[i]; }
    pieceX += shiftX;
    pieceY += shiftY;
    dirty = 1;
}

// Clear horizontal/vertical runs of >=3 same color, but only runs that include
// at least one block NOT just placed (so a piece can't clear against itself).
void resolveMatches(void)
{
    u8 clear[GRID_W][GRID_H];
    u8 x, y, run, c, i, hasOld;
    u16 n = 0;

    for (x = 0; x < GRID_W; x++)
        for (y = 0; y < GRID_H; y++)
            clear[x][y] = 0;

    for (y = 0; y < GRID_H; y++)     // horizontal
    {
        x = 0;
        while (x < GRID_W)
        {
            c = field[x][y];
            if (c == EMPTY) { x++; continue; }
            run = 1;
            while (x + run < GRID_W && field[x + run][y] == c) run++;
            if (run >= 3)
            {
                hasOld = 0;
                for (i = 0; i < run; i++) if (!justPlaced[x + i][y]) { hasOld = 1; break; }
                if (hasOld) for (i = 0; i < run; i++) clear[x + i][y] = 1;
            }
            x += run;
        }
    }
    for (x = 0; x < GRID_W; x++)     // vertical
    {
        y = 0;
        while (y < GRID_H)
        {
            c = field[x][y];
            if (c == EMPTY) { y++; continue; }
            run = 1;
            while (y + run < GRID_H && field[x][y + run] == c) run++;
            if (run >= 3)
            {
                hasOld = 0;
                for (i = 0; i < run; i++) if (!justPlaced[x][y + i]) { hasOld = 1; break; }
                if (hasOld) for (i = 0; i < run; i++) clear[x][y + i] = 1;
            }
            y += run;
        }
    }

    for (x = 0; x < GRID_W; x++)
        for (y = 0; y < GRID_H; y++)
            if (clear[x][y]) { field[x][y] = EMPTY; n++; }

    if (n > 0)
    {
        score += n * 50;
        if (n > 3) score += (n - 3) * 100;
        dirty = 1;
    }
}

void placePiece(void)
{
    u8 i;
    if (!canPlace())
        return; // overlap -> rejected

    for (i = 0; i < cellCount; i++)
    {
        field[pieceX + curX[i]][pieceY + curY[i]] = cellColor[i] + 1;
        justPlaced[pieceX + curX[i]][pieceY + curY[i]] = 1;
    }

    resolveMatches();

    for (i = 0; i < cellCount; i++)
        justPlaced[pieceX + curX[i]][pieceY + curY[i]] = 0;

    spawnPiece();
}

//---------------------------------------------------------------------------------
// consoleDrawText %d is unreliable here; format digits manually and print %s.
void formatNum(u16 n, char *buf, u8 digits)
{
    buf[digits] = 0;
    while (digits > 0) { digits--; buf[digits] = '0' + (n % 10); n /= 10; }
}

void drawStatic(void)
{
    u8 y;
    consoleDrawText(9, 2, "POMPOM TETRIS - SNES");
    consoleDrawText(1, 5, "DPAD MOVE  L/Y R/X ROT  A PLACE");

    consoleDrawText(GRID_X - 1, GRID_Y - 1, "###########");
    consoleDrawText(GRID_X - 1, GRID_Y + GRID_H, "###########");
    for (y = 0; y < GRID_H; y++)
    {
        consoleDrawText(GRID_X - 1, GRID_Y + y, "#");
        consoleDrawText(GRID_X + GRID_W, GRID_Y + y, "#");
    }
}

void drawGrid(void)
{
    char cells[GRID_W][GRID_H];
    char row[GRID_W + 1];
    u8 x, y, i;

    for (y = 0; y < GRID_H; y++)
        for (x = 0; x < GRID_W; x++)
            cells[x][y] = (field[x][y] != EMPTY) ? PLACED_CH[field[x][y] - 1] : '.';

    for (i = 0; i < cellCount; i++)
    {
        int cx = pieceX + curX[i];
        int cy = pieceY + curY[i];
        if (cx >= 0 && cx < GRID_W && cy >= 0 && cy < GRID_H)
            cells[cx][cy] = (field[cx][cy] != EMPTY) ? '*' : ACTIVE_CH[cellColor[i]];
    }

    for (y = 0; y < GRID_H; y++)
    {
        for (x = 0; x < GRID_W; x++)
            row[x] = cells[x][y];
        row[GRID_W] = 0;
        consoleDrawText(GRID_X, GRID_Y + y, "%s", row);
    }

    consoleDrawText(GRID_X - 1, GRID_Y + GRID_H + 2,
                    canPlace() ? "PLACE: OK " : "PLACE: NO ");
    {
        char sbuf[7];
        formatNum(score, sbuf, 6);
        consoleDrawText(1, 3, "SCORE %s", sbuf);
    }
}

//---------------------------------------------------------------------------------
int main(void)
{
    unsigned short down;

    consoleInitDefaultText(0);
    bgSetGfxPtr(0, 0x3000);
    bgSetMapPtr(0, 0x6800, SC_32x32);
    setMode(BG_MODE1, 0);
    bgSetDisable(1);
    bgSetDisable(2);

    srand(0x1234); // fixed seed for now (deterministic); randomize in a later phase
    clearBoard();
    score = 0;
    spawnPiece();
    drawStatic();
    setScreenOn();

    // Discard spurious pad edges from the first few frames after boot.
    {
        u8 k;
        for (k = 0; k < 4; k++) { WaitForVBlank(); (void)padsDown(0); }
    }

    while (1)
    {
        down = padsDown(0);

        if (down & KEY_UP)    tryMove(0, -1);
        if (down & KEY_DOWN)  tryMove(0, 1);
        if (down & KEY_LEFT)  tryMove(-1, 0);
        if (down & KEY_RIGHT) tryMove(1, 0);
        if (down & (KEY_R | KEY_X)) tryRotate(1);
        if (down & (KEY_L | KEY_Y)) tryRotate(-1);
        if (down & KEY_A)     placePiece();

        if (dirty)
        {
            drawGrid();
            dirty = 0;
        }

        WaitForVBlank();
    }
    return 0;
}
