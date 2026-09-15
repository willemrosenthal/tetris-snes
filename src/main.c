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
#define WILD 99      /* field value / cellColor marker: matches any color */
#define BOMB 98      /* cellColor marker for an active bomb piece */
#define WILD_CHANCE 5  /* percent, per cell (prefab wildChance 0.05) */
#define BOMB_CHANCE 10 /* percent, per piece */

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
u8 cellColor[MAX_CELLS];             // per-cell color 0..GEN_COLORS-1, or WILD/BOMB
int pieceX, pieceY;                  // anchor cell
u8 pieceIsBomb;                      // current piece is a bomb

u8 dirty;
u16 score;

// Marble queue + spawn timer + lose condition (port of BlockLibrary)
#define TUBE_MAX 5    // 5 marbles queued = game over
#define SPAWN_MAX 300 // ~5.0s at 60Hz (marbleSpawnMaxTime)
#define SPAWN_MIN 57  // ~0.95s (marbleSpawnMinTime)
u8 queue;         // marbles waiting in the tube
u8 hasActive;     // an active piece is currently in play
u8 gameOver;
u16 spawnTimer;   // frames until the next marble
u16 spawnInterval;
u8 speedPct;      // 0..100 acceleration (marbleSpawnTimer speeds up over time)

//---------------------------------------------------------------------------------
// Phase 5b layers (pixel-accurate rework):
//   bg0 (BG1hw, 16-color, front-most BG): blocks
//   bg1 (BG2hw, 16-color, behind blocks): the real background SCENE
//     (checker + play-area grid + frame), converted whole from the Unity assets
//   bg2 (BG3, 2bpp, high priority): console HUD text (font is 2-color)
extern char blocktiles, blocktiles_end;
extern char bombtiles, bombtiles_end;
extern char bombpal, bombpal_end;
extern char scenetiles, scenetiles_end;
extern char scenemap, scenemap_end;
extern char scenepal, scenepal_end;
extern char font2tiles, font2tiles_end;
extern char font2pal, font2pal_end;
extern char reticletiles, reticletiles_end;
extern char reticlepal, reticlepal_end;

// Placement reticle sprites (OBJ): one 16x16 sprite per active-piece cell,
// overlaying the piece so it's clearly "being placed" vs settled blocks.
#define OBJ_CHR 0x4000   // OBJ tile VRAM (free 0x3700..0x5000, 0x2000-aligned)
#define OBJ_PAL 0        // OBJ palette 0 (CGRAM 128-143): white reticle
#define OBJ_PAL_RED 1    // OBJ palette 1: red reticle (invalid placement)
// Play-area top-left pixel; cell (cx,cy) -> screen (PF_PX+cx*16, PF_PY+cy*16).
#define PF_PX (PF_TX * 8)
#define PF_PY (PF_TY * 8)

#define BLK_CHR 0x0000   // bg0 block tiles (tile 0 = blank)
#define BLK_MAP 0x5000   // bg0 block tilemap
#define SCN_CHR 0x2000   // bg1 scene tiles (0x1000-aligned)
#define SCN_MAP 0x3000   // bg1 scene tilemap (0x400-aligned)
#define TXT_CHR 0x6000   // bg2 console font tiles (0x1000-aligned)
#define TXT_MAP 0x6800   // bg2 console tilemap (0x400-aligned)
#define SCN_PAL 6        // background scene uses BG palette 6 (CGRAM 96-111)

#define PF_TX 2          // playfield origin in 8x8 tiles (left)
#define PF_TY 3          // playfield origin (top). 9x12 blocks = 18x24 tiles.

// SNES BGR15 color from 8-bit RGB
#define RGB15(r, g, b) (((u16)((b) >> 3) << 10) | ((u16)((g) >> 3) << 5) | ((r) >> 3))

// Per-color palettes sampled from tetris-blocks.png. Order MUST match how
// gfx4snes indexed the (transparency-reserved) master tile's pixels:
//  idx0 = transparent, 1 = BLACK outline, 2 = DARK, 3 = LIGHT, 4 = WHITE, 5 = MID.
// The game's 5 colors are blockType 0..4 (Block.prefab blockArt, totalColors=5):
//  0=green, 1=blue, 2=pink, 3=purple, 4=orange. -> palette slots 1..5.
#define BLK RGB15(0, 0, 0)
#define WHT RGB15(252, 252, 252)
const u16 BLOCK_PAL[5][6] = {
    //  0(transp) 1=black 2=dark            3=light             4=white 5=mid
    {0, BLK, RGB15(0,107,0),    RGB15(0,255,0),     WHT, RGB15(0,180,0)   }, // 0 green
    {0, BLK, RGB15(125,62,242), RGB15(64,248,248),  WHT, RGB15(152,96,255)}, // 1 blue
    {0, BLK, RGB15(248,0,144),  RGB15(248,128,184), WHT, RGB15(248,24,96) }, // 2 pink
    {0, BLK, RGB15(61,0,124),   RGB15(126,0,255),   WHT, RGB15(86,0,174)  }, // 3 purple
    {0, BLK, RGB15(200,56,0),   RGB15(248,184,0),   WHT, RGB15(248,120,0) }, // 4 orange
};

u16 bg1map[32 * 32];  // RAM copy of BG1 tilemap; DMA'd to VRAM on change

// Bomb is now a hardware sprite (OBJ), freeing BG palette 7 for wild's yellow.
// OBJ tiles loaded at OBJ tile 32 (after the reticle's 0..31); frame f sprite
// gfxoffset = BOMB_OBJ_T0 + f*2. Uses OBJ palette 2.
#define BOMB_OBJ_T0 32
#define BOMB_OBJ_PAL 2

// Animation timing from the Unity clips (60fps == our frame rate). Per-frame
// hold times VARY, so we drive each from a per-frame lookup indexed by a clock.
// bomb block.anim: bomb1(4f), bomb2(4f), bomb3(3f)  -> 11-frame loop.
#define BOMB_LEN 11
const u8 BOMB_SEQ[BOMB_LEN] = {0,0,0,0, 1,1,1,1, 2,2,2};
// Wild: flash as fast as possible -- 1 frame per color. Wild blocks always use
// ONE palette slot (WILD_SLOT); we animate by cycling THAT palette's colors each
// frame (cheap CGRAM writes) instead of rebuilding the tilemap -- so the wild
// flash costs nothing per frame. Exact clip order yellow,pink,purple,blue,green.
#define WILD_SLOT 7
#define WILD_LEN 5
const u16 WILD_SH[WILD_LEN][3] = { // {dark, light, mid} per color
    {RGB15(248,120,0), RGB15(248,248,0),  RGB15(248,184,0)}, // yellow
    {RGB15(248,0,144), RGB15(248,128,184),RGB15(248,24,96) }, // pink
    {RGB15(61,0,124),  RGB15(126,0,255),  RGB15(86,0,174)  }, // purple
    {RGB15(125,62,242),RGB15(64,248,248), RGB15(152,96,255)}, // blue
    {RGB15(0,107,0),   RGB15(0,255,0),    RGB15(0,180,0)   }, // green
};

// Animation state (advanced each frame in the main loop).
u16 animClock;
u8 bombFrame;   // 0..2  -> BOMB_SEQ[animClock % BOMB_LEN]

// D-pad auto-repeat: move once on press, then repeat while held.
#define REPEAT_INITIAL 10  // frames before auto-repeat starts
#define REPEAT_RATE 3      // frames between repeats
u8 moveDelay;

// Cycle the wild palette (slot 7) to the next color. Cheap; call each vblank.
void cycleWild(void)
{
    u8 c = animClock % WILD_LEN;
    setPaletteColor(WILD_SLOT * 16 + 2, WILD_SH[c][0]); // dark
    setPaletteColor(WILD_SLOT * 16 + 3, WILD_SH[c][1]); // light
    setPaletteColor(WILD_SLOT * 16 + 5, WILD_SH[c][2]); // mid
}

// Placement "pop": a brief reticle flash at the cells a piece just landed on
// (feedback, since BG tiles can't scale like the Unity 1.2x pop). Uses OBJ
// sprites 4..7. popTimer counts down frames.
#define POP_FRAMES 8
u8 popTimer;
u8 popN;
s8 popX[4], popY[4];

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

    pieceIsBomb = (randn(100) < BOMB_CHANCE);
    if (pieceIsBomb)
    {
        cellCount = 1;
        curX[0] = 0; curY[0] = 0;
        cellColor[0] = BOMB;
        pieceX = GRID_W / 2;
        pieceY = 0;
        dirty = 1;
        return;
    }

    s = randn(NUM_SHAPES);
    cellCount = SHAPES[s].n;
    for (i = 0; i < cellCount; i++)
    {
        curX[i] = SHAPES[s].x[i];
        curY[i] = SHAPES[s].y[i];
        if (curX[i] > maxx) maxx = curX[i];
    }

    // Pick 2 colors; each cell is one of the two, with a small wild chance
    // (per TetrisBlockGroup.ChooseBricks).
    c0 = randn(GEN_COLORS);
    c1 = randn(GEN_COLORS);
    for (i = 0; i < cellCount; i++)
        cellColor[i] = (randn(100) < WILD_CHANCE) ? WILD : ((rand() & 1) ? c1 : c0);

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

// A cell matches color-run value v if it is that color or a wild (which stands
// in for any color). Wild cells never initiate a run (matches TetrisBlock: wilds
// return early), but they are swept up into a run they help complete.
#define MATCHES(cell, v) ((cell) == (v) || (cell) == WILD)

// Clear horizontal/vertical runs of >=3 (same color, wilds acting as bridges).
// A run is SEEDED if it connects the just-placed piece to existing blocks (has
// both a just-placed cell and a pre-existing cell) -- this prevents a piece from
// self-clearing. Clearing then PROPAGATES: any >=3 run that shares a cell with an
// already-clearing run also clears (transitive), matching the Unity flood. This
// makes a completed plus (+) clear BOTH crossing rows, not just the seeded one.
void resolveMatches(void)
{
    u8 clear[GRID_W][GRID_H];
    u8 x, y, v, hasOld, hasNew, hits, changed;
    int a, b, i;
    u16 n = 0;

    for (x = 0; x < GRID_W; x++)
        for (y = 0; y < GRID_H; y++)
            clear[x][y] = 0;

    do
    {
        changed = 0;

        for (y = 0; y < GRID_H; y++)          // horizontal runs
            for (x = 0; x < GRID_W; x++)
            {
                v = field[x][y];
                if (v == EMPTY || v == WILD) continue; // only real colors initiate
                a = x; b = x;
                while (a - 1 >= 0 && MATCHES(field[a - 1][y], v)) a--;
                while (b + 1 < GRID_W && MATCHES(field[b + 1][y], v)) b++;
                if (b - a + 1 < 3) continue;
                hasOld = hasNew = hits = 0;
                for (i = a; i <= b; i++)
                {
                    if (justPlaced[i][y]) hasNew = 1; else hasOld = 1;
                    if (clear[i][y]) hits = 1;
                }
                if ((hasNew && hasOld) || hits)         // seed or propagate
                    for (i = a; i <= b; i++)
                        if (!clear[i][y]) { clear[i][y] = 1; changed = 1; }
            }

        for (x = 0; x < GRID_W; x++)          // vertical runs
            for (y = 0; y < GRID_H; y++)
            {
                v = field[x][y];
                if (v == EMPTY || v == WILD) continue;
                a = y; b = y;
                while (a - 1 >= 0 && MATCHES(field[x][a - 1], v)) a--;
                while (b + 1 < GRID_H && MATCHES(field[x][b + 1], v)) b++;
                if (b - a + 1 < 3) continue;
                hasOld = hasNew = hits = 0;
                for (i = a; i <= b; i++)
                {
                    if (justPlaced[x][i]) hasNew = 1; else hasOld = 1;
                    if (clear[x][i]) hits = 1;
                }
                if ((hasNew && hasOld) || hits)
                    for (i = a; i <= b; i++)
                        if (!clear[x][i]) { clear[x][i] = 1; changed = 1; }
            }
    } while (changed);

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

// Bomb: clear this cell and its 8 neighbors (per TetrisBlock.BombExplode).
void explodeBomb(int bx, int by)
{
    int dx, dy, x, y;
    u16 n = 0;
    for (dx = -1; dx <= 1; dx++)
        for (dy = -1; dy <= 1; dy++)
        {
            x = bx + dx; y = by + dy;
            if (x >= 0 && x < GRID_W && y >= 0 && y < GRID_H && field[x][y] != EMPTY)
            {
                field[x][y] = EMPTY;
                n++;
            }
        }
    if (n > 0) { score += n * 50; dirty = 1; }
}

// Returns 1 if the piece was placed (caller pulls the next one from the queue).
u8 placePiece(void)
{
    u8 i;
    if (!canPlace())
        return 0; // overlap -> rejected

    if (pieceIsBomb)
    {
        int bx = pieceX + curX[0], by = pieceY + curY[0];
        field[bx][by] = 1;      // temp non-empty so the bomb cell is counted
        explodeBomb(bx, by);
        return 1;
    }

    for (i = 0; i < cellCount; i++)
    {
        field[pieceX + curX[i]][pieceY + curY[i]] =
            (cellColor[i] == WILD) ? WILD : cellColor[i] + 1;
        justPlaced[pieceX + curX[i]][pieceY + curY[i]] = 1;
    }

    // Record cells for the placement pop flash.
    popN = cellCount;
    for (i = 0; i < cellCount; i++)
    {
        popX[i] = pieceX + curX[i];
        popY[i] = pieceY + curY[i];
    }
    popTimer = POP_FRAMES;

    resolveMatches();

    for (i = 0; i < cellCount; i++)
        justPlaced[pieceX + curX[i]][pieceY + curY[i]] = 0;

    return 1;
}

// A marble arrives in the tube; the spawn interval accelerates each time.
// Filling the tube (TUBE_MAX) ends the game.
void spawnMarble(void)
{
    queue++;
    dirty = 1;
    if (queue >= TUBE_MAX)
    {
        gameOver = 1;
        return;
    }
    if (speedPct < 100) speedPct++;
    spawnInterval = SPAWN_MAX - (u16)(((u32)(SPAWN_MAX - SPAWN_MIN) * speedPct) / 100);
    spawnTimer = spawnInterval;
}

void startGame(void)
{
    clearBoard();
    score = 0;
    queue = 0;
    speedPct = 0;
    gameOver = 0;
    spawnInterval = SPAWN_MAX;
    spawnTimer = spawnInterval;
    animClock = 0; bombFrame = BOMB_SEQ[0];
    popTimer = 0; popN = 0; // crt0 doesn't zero BSS; avoid garbage pop sprites
    spawnPiece();     // first piece is free
    hasActive = 1;
    dirty = 1;

}

//---------------------------------------------------------------------------------
// consoleDrawText %d is unreliable here; format digits manually and print %s.
void formatNum(u16 n, char *buf, u8 digits)
{
    buf[digits] = 0;
    while (digits > 0) { digits--; buf[digits] = '0' + (n % 10); n /= 10; }
}

// Palette slot (1..5) for a *stored field value* (color+1, i.e. 1..5, or WILD).
// Wild cycles through all block palettes over time (rainbow flash, like the
// Unity wild animation).
u8 slotForField(u8 v)
{
    if (v == WILD) return WILD_SLOT;
    return (v >= 1 && v <= 5) ? v : 5;
}

// Palette slot (1..5) for an *active cellColor* (raw color 0..4, or WILD).
u8 slotForColor(u8 cc)
{
    if (cc == WILD) return WILD_SLOT;
    return (cc <= 4) ? (cc + 1) : 5;
}

// One SNES tilemap entry for a block sub-tile (tileNo relative to block tiles).
#define BLOCK_ENTRY(subtile, slot) ((u16)((subtile) + 1) | ((u16)(slot) << 10))

void gfxInit(void)
{
    u8 c, k;

    // --- bg0: blocks (16-color, front-most BG) ---
    // Block tiles at VRAM tile #1 (tile 0 stays blank). Bomb is a sprite now.
    dmaCopyVram((u8 *)&blocktiles, BLK_CHR + 16, (u16)(&blocktiles_end - &blocktiles));
    bgSetGfxPtr(0, BLK_CHR);
    bgSetMapPtr(0, BLK_MAP, SC_32x32);
    // 5 block palettes -> slots 1..5. (setPaletteColor is multi-statement: braces!)
    for (c = 0; c < 5; c++)
        for (k = 0; k < 6; k++)
        {
            setPaletteColor((c + 1) * 16 + k, BLOCK_PAL[c][k]);
        }
    // Slot 7 = the WILD palette. Static outline/white; the 3 shades (idx2,3,5)
    // are cycled each frame by cycleWild() for the rainbow flash.
    setPaletteColor(WILD_SLOT * 16 + 1, BLK); // black outline
    setPaletteColor(WILD_SLOT * 16 + 4, WHT); // white
    cycleWild();                              // seed shades for the first frame

    // --- bg1: pixel-accurate background scene (16-color, behind blocks) ---
    dmaCopyVram((u8 *)&scenetiles, SCN_CHR, (u16)(&scenetiles_end - &scenetiles));
    dmaCopyVram((u8 *)&scenemap, SCN_MAP, (u16)(&scenemap_end - &scenemap));
    setPalette((u8 *)&scenepal, SCN_PAL * 16, 16 * 2); // scene palette -> slot 6
    bgSetGfxPtr(1, SCN_CHR);
    bgSetMapPtr(1, SCN_MAP, SC_32x32);
    // scene idx0 = checker-light and is transparent on BG; make the backdrop that
    // color so the checker's light squares read correctly.
    setPaletteColor(0, RGB15(125, 111, 201));
    setPaletteColor(1, RGB15(248, 248, 248)); // HUD font text = white (palette 0 idx1)

    // --- OBJ: placement reticle sprite (16x16) ---
    oamInit();
    oamInitGfxSet((u8 *)&reticletiles, (u16)(&reticletiles_end - &reticletiles),
                  (u8 *)&reticlepal, (u16)(&reticlepal_end - &reticlepal),
                  OBJ_PAL, OBJ_CHR, OBJ_SIZE16_L32);
    // OBJ palette 1 = RED reticle (shown when placement is invalid/overlapping).
    // reticle tiles use idx1 (white) + idx2 (black); recolor idx1 to red.
    setPaletteColor(128 + OBJ_PAL_RED * 16 + 1, RGB15(248, 24, 24)); // red
    setPaletteColor(128 + OBJ_PAL_RED * 16 + 2, RGB15(80, 0, 0));    // dark red

    // Bomb OBJ tiles at OBJ tile 32 (after the reticle's 32-tile block) + palette 2.
    dmaCopyVram((u8 *)&bombtiles, OBJ_CHR + BOMB_OBJ_T0 * 16,
                (u16)(&bombtiles_end - &bombtiles));
    setPalette((u8 *)&bombpal, 128 + BOMB_OBJ_PAL * 16, 16 * 2);

    oamClear(0, 0); // hide all sprites initially
}

// Show one 16x16 reticle sprite over each active-piece cell (hide the rest).
// Not shown for the bomb (its own art) or when there's no active piece.
// Sprite id 8 (OAM byte 32) = the bomb sprite. Reticles use ids 0..3.
#define BOMB_OAM 32

void renderReticle(void)
{
    u8 i;
    u8 activePiece = hasActive && !gameOver && !pieceIsBomb;

    // Reticles (one per active non-bomb cell), red when placement is invalid.
    if (activePiece)
    {
        u8 pal = canPlace() ? OBJ_PAL : OBJ_PAL_RED;
        for (i = 0; i < cellCount; i++)
        {
            int cx = pieceX + curX[i];
            int cy = pieceY + curY[i];
            oamSet(i * 4, PF_PX + cx * 16, PF_PY + cy * 16, 2, 0, 0, 0, pal);
            oamSetEx(i * 4, OBJ_SMALL, OBJ_SHOW);
        }
        for (i = cellCount; i < 4; i++) oamSetVisible(i * 4, OBJ_HIDE);
    }
    else
        for (i = 0; i < 4; i++) oamSetVisible(i * 4, OBJ_HIDE);

    // Bomb sprite (animated), drawn only while a bomb piece is in play.
    if (hasActive && !gameOver && pieceIsBomb)
    {
        int cx = pieceX + curX[0];
        int cy = pieceY + curY[0];
        oamSet(BOMB_OAM, PF_PX + cx * 16, PF_PY + cy * 16, 2, 0, 0,
               BOMB_OBJ_T0 + bombFrame * 2, BOMB_OBJ_PAL);
        oamSetEx(BOMB_OAM, OBJ_SMALL, OBJ_SHOW);
    }
    else
        oamSetVisible(BOMB_OAM, OBJ_HIDE);

    // Placement pop: flash a white reticle at the just-placed cells (sprites 4..7).
    if (popTimer && (popTimer & 2)) // blink on/off as it counts down
    {
        for (i = 0; i < popN; i++)
        {
            oamSet((4 + i) * 4, PF_PX + popX[i] * 16, PF_PY + popY[i] * 16,
                   2, 0, 0, 0, OBJ_PAL);
            oamSetEx((4 + i) * 4, OBJ_SMALL, OBJ_SHOW);
        }
        for (i = popN; i < 4; i++) oamSetVisible((4 + i) * 4, OBJ_HIDE);
    }
    else
        for (i = 0; i < 4; i++) oamSetVisible((4 + i) * 4, OBJ_HIDE);
}

void drawStatic(void)
{
    consoleDrawText(10, 0, "POMPOM TETRIS");
    consoleDrawText(0, 27, "DPAD MOVE  L/Y R/X ROT  A PLACE");
}

// Place a 16x16 block (2x2 tiles) into the bg1 map buffer at grid cell (gx,gy).
void putBlock(u8 gx, u8 gy, u8 slot)
{
    u16 tx = PF_TX + gx * 2;
    u16 ty = PF_TY + gy * 2;
    u16 base = ty * 32 + tx;
    bg1map[base]          = BLOCK_ENTRY(0, slot); // TL
    bg1map[base + 1]      = BLOCK_ENTRY(1, slot); // TR
    bg1map[base + 32]     = BLOCK_ENTRY(2, slot); // BL
    bg1map[base + 33]     = BLOCK_ENTRY(3, slot); // BR
}


// Rebuild the BG1 tilemap from the field + active piece (blocks are graphics).
void renderBlocks(void)
{
    u16 i;
    u8 x, y;

    for (i = 0; i < 32 * 32; i++) bg1map[i] = 0; // clear (tile 0 = blank)

    for (x = 0; x < GRID_W; x++)
        for (y = 0; y < GRID_H; y++)
            if (field[x][y] != EMPTY)
                putBlock(x, y, slotForField(field[x][y]));

    // The bomb is drawn as a sprite (see renderReticle), not on the BG.
    if (hasActive && !gameOver && !pieceIsBomb)
    {
            for (i = 0; i < cellCount; i++)
            {
                int cx = pieceX + curX[i];
                int cy = pieceY + curY[i];
                if (cx >= 0 && cx < GRID_W && cy >= 0 && cy < GRID_H &&
                    field[cx][cy] == EMPTY)
                    putBlock((u8)cx, (u8)cy, slotForColor(cellColor[i]));
            }
    }
}

// HUD text (console, bg 0) to the right of the playfield.
void drawHud(void)
{
    char sbuf[7], tbuf[2];
    formatNum(score, sbuf, 6);
    consoleDrawText(22, 4, "SCORE");
    consoleDrawText(22, 5, sbuf);
    tbuf[0] = '0' + queue;
    tbuf[1] = 0;
    consoleDrawText(22, 8, "TUBE %s/5", tbuf);
    consoleDrawText(22, 10, canPlace() ? "PLACE OK" : "PLACE NO");
    consoleDrawText(22, 13, gameOver ? "GAME OVER" : "         ");
    consoleDrawText(22, 14, gameOver ? "-START-  " : "         ");
}

void drawGrid(void)
{
    renderBlocks();
    drawHud();
}

//---------------------------------------------------------------------------------
int main(void)
{
    unsigned short down;

    // Console HUD -> bg2 (BG3, 4-color). Use a 2bpp font (the default is 4bpp,
    // which garbles on BG3). Point font gfx/map at BG3 VRAM, offset 0.
    consoleSetTextGfxPtr(TXT_CHR);
    consoleSetTextMapPtr(TXT_MAP);
    consoleSetTextOffset(0);
    consoleInitText(0, 4, &font2tiles, &font2pal); // 2 colors (4 bytes)
    bgSetGfxPtr(2, TXT_CHR);
    bgSetMapPtr(2, TXT_MAP, SC_32x32);

    gfxInit();          // bg0 blocks, bg1 background scene
    setMode(BG_MODE1, BG3_MODE1_PRIORITY_HIGH); // BG3 (text) on top
    bgSetEnable(0);     // blocks
    bgSetEnable(1);     // scene
    bgSetEnable(2);     // text

    srand(0x1234); // fixed seed for now (deterministic); randomize in a later phase
    startGame();
    drawStatic();
    drawGrid();
    renderReticle();
    dmaCopyVram((u8 *)bg1map, BLK_MAP, sizeof(bg1map));
    setScreenOn();

    // Discard spurious pad edges from the first few frames after boot.
    {
        u8 k;
        for (k = 0; k < 4; k++) { WaitForVBlank(); (void)padsDown(0); }
    }

    while (1)
    {
        unsigned short held;
        down = padsDown(0);
        held = padsCurrent(0);

        if (gameOver)
        {
            if (down & (KEY_START | KEY_A)) startGame();
        }
        else
        {
            // Marble spawn timer. When the tube is empty and we have no piece,
            // spawn immediately so the player is never starved (like the original).
            if (queue == 0 && !hasActive) spawnTimer = 0;
            if (spawnTimer > 0) spawnTimer--;
            if (spawnTimer == 0) spawnMarble();

            // Pull the next piece from the tube when we have none
            if (!hasActive && queue > 0 && !gameOver)
            {
                queue--;
                spawnPiece();
                hasActive = 1;
                dirty = 1;
            }

            if (hasActive)
            {
                // D-pad movement with auto-repeat (press once, then repeat held).
                int dx = (held & KEY_LEFT) ? -1 : (held & KEY_RIGHT) ? 1 : 0;
                int dy = (held & KEY_UP) ? -1 : (held & KEY_DOWN) ? 1 : 0;
                if (dx || dy)
                {
                    if (down & (KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN))
                    {
                        tryMove(dx, dy);          // immediate on fresh press
                        moveDelay = REPEAT_INITIAL;
                    }
                    else if (moveDelay > 0 && --moveDelay == 0)
                    {
                        tryMove(dx, dy);          // auto-repeat
                        moveDelay = REPEAT_RATE;
                    }
                }
                else
                    moveDelay = 0;

                if (down & (KEY_R | KEY_X)) tryRotate(1);
                if (down & (KEY_L | KEY_Y)) tryRotate(-1);
                if (down & KEY_A)
                {
                    if (placePiece()) { hasActive = 0; dirty = 1; }
                }
            }
        }

        // Per-frame animation is CHEAP (sprites + one palette write): the bomb
        // sprite frame, the placement pop, and the wild palette cycle. These do
        // NOT rebuild the tilemap, so they don't force the expensive redraw.
        animClock++;
        bombFrame = BOMB_SEQ[animClock % BOMB_LEN];
        if (popTimer) popTimer--;

        if (dirty) drawGrid();   // rebuild tilemap + HUD only on real changes
        renderReticle();         // reticle + bomb sprite + pop (OAM), every frame

        WaitForVBlank();
        cycleWild();             // wild rainbow via palette (CGRAM), every frame
        if (dirty)
        {
            dmaCopyVram((u8 *)bg1map, BLK_MAP, sizeof(bg1map));
            dirty = 0;
        }
    }
    return 0;
}
