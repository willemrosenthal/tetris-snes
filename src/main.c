/*---------------------------------------------------------------------------------
    Pompom Tetris minigame -> SNES port

    Phase 2: grid data model + one controllable piece.
      - D-pad moves the active piece around the 9x12 grid (kept in-bounds).
      - L / Y rotate CCW, R / X rotate CW (matches TetrisGamepadControls).
      - A places the piece on empty cells; overlaps are rejected.
    Rendering still uses the text console (tile/sprite art comes in Phase 5):
      placed blocks = lowercase color letters, active piece = uppercase,
      '*' marks a cell where the active piece overlaps a placed block.
---------------------------------------------------------------------------------*/
#include <snes.h>

#define GRID_W 9
#define GRID_H 12
#define GRID_X 11 /* console column of the grid interior's left edge  */
#define GRID_Y 8  /* console row of the grid interior's top edge      */
#define EMPTY 0
#define NUM_COLORS 7

// field[x][y]: 0 = empty, otherwise (color index + 1)
u8 field[GRID_W][GRID_H];

// Color glyphs: placed (lowercase) vs active piece (uppercase)
const char PLACED_CH[NUM_COLORS] = {'r', 'g', 'b', 'y', 'p', 'c', 'o'};
const char ACTIVE_CH[NUM_COLORS] = {'R', 'G', 'B', 'Y', 'P', 'C', 'O'};

// Base shape (L-tetromino), offsets from the piece anchor. Asymmetric so
// rotation is clearly visible. Phase 4 replaces this with real piece data.
const s8 BASE_X[4] = {0, 0, 0, 1};
const s8 BASE_Y[4] = {-1, 0, 1, 1};

// Active piece state
s8 curX[4], curY[4]; // current (possibly rotated) offsets
int pieceX, pieceY;  // anchor cell on the grid
u8 pieceColor;       // 0..NUM_COLORS-1

u8 dirty; // redraw grid interior when set

//---------------------------------------------------------------------------------
// True if every piece cell (anchor + offsets) sits inside the grid.
u8 cellsInBounds(int ax, int ay, s8 *ox, s8 *oy)
{
    u8 i;
    for (i = 0; i < 4; i++)
    {
        int cx = ax + ox[i];
        int cy = ay + oy[i];
        if (cx < 0 || cx >= GRID_W || cy < 0 || cy >= GRID_H)
            return 0;
    }
    return 1;
}

// True if every piece cell is on an empty grid square.
u8 canPlace(void)
{
    u8 i;
    for (i = 0; i < 4; i++)
    {
        int cx = pieceX + curX[i];
        int cy = pieceY + curY[i];
        if (field[cx][cy] != EMPTY)
            return 0;
    }
    return 1;
}

// SNES crt0 does not zero-initialize globals, so clear the field explicitly.
void clearField(void)
{
    u8 x, y;
    for (x = 0; x < GRID_W; x++)
        for (y = 0; y < GRID_H; y++)
            field[x][y] = EMPTY;
}

void resetPiece(void)
{
    u8 i;
    for (i = 0; i < 4; i++)
    {
        curX[i] = BASE_X[i];
        curY[i] = BASE_Y[i];
    }
    pieceX = 4;
    pieceY = 1;
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

// dir > 0 = clockwise, dir < 0 = counter-clockwise. Includes a simple wall kick
// (shift the anchor back in-bounds after rotating), mirroring the Unity version.
void tryRotate(int dir)
{
    s8 rx[4], ry[4];
    int minx = 127, maxx = -128, miny = 127, maxy = -128;
    int shiftX = 0, shiftY = 0;
    u8 i;

    for (i = 0; i < 4; i++)
    {
        if (dir > 0)
        {
            rx[i] = curY[i];
            ry[i] = -curX[i];
        }
        else
        {
            rx[i] = -curY[i];
            ry[i] = curX[i];
        }
    }

    // Where would the rotated cells land? Compute the shift needed to pull them
    // fully back inside the grid.
    for (i = 0; i < 4; i++)
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
        return; // shouldn't happen for a 4-cell piece, but bail rather than glitch

    for (i = 0; i < 4; i++)
    {
        curX[i] = rx[i];
        curY[i] = ry[i];
    }
    pieceX += shiftX;
    pieceY += shiftY;
    dirty = 1;
}

void placePiece(void)
{
    u8 i;
    if (!canPlace())
        return; // overlap -> rejected
    for (i = 0; i < 4; i++)
        field[pieceX + curX[i]][pieceY + curY[i]] = pieceColor + 1;

    pieceColor = (pieceColor + 1) % NUM_COLORS; // cycle so the grid shows variety
    resetPiece();
}

//---------------------------------------------------------------------------------
void drawStatic(void)
{
    u8 y;
    consoleDrawText(9, 2, "POMPOM TETRIS - SNES");
    consoleDrawText(1, 5, "DPAD MOVE  L/Y R/X ROT  A PLACE");

    // Grid border (never changes)
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

    // Overlay the active piece; '*' where it sits on a placed block (invalid).
    for (i = 0; i < 4; i++)
    {
        int cx = pieceX + curX[i];
        int cy = pieceY + curY[i];
        if (cx >= 0 && cx < GRID_W && cy >= 0 && cy < GRID_H)
            cells[cx][cy] = (field[cx][cy] != EMPTY) ? '*' : ACTIVE_CH[pieceColor];
    }

    for (y = 0; y < GRID_H; y++)
    {
        for (x = 0; x < GRID_W; x++)
            row[x] = cells[x][y];
        row[GRID_W] = 0;
        consoleDrawText(GRID_X, GRID_Y + y, "%s", row);
    }

    // Placement status line
    consoleDrawText(GRID_X - 1, GRID_Y + GRID_H + 2,
                    canPlace() ? "PLACE: OK " : "PLACE: NO ");
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

    clearField();
    pieceColor = 0;
    resetPiece();
    drawStatic();
    setScreenOn();

    // Discard spurious pad edges from the first few frames before the pad is
    // first scanned (otherwise the piece jumps on boot).
    {
        u8 k;
        for (k = 0; k < 4; k++)
        {
            WaitForVBlank();
            (void)padsDown(0);
        }
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
