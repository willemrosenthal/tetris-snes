/*---------------------------------------------------------------------------------
    Pompom Tetris minigame -> SNES port
    Phase 1: project skeleton. Boots and draws the 9x12 play grid + HUD labels
    using the text console, to prove out the build/run/verify pipeline.
    (Tile/sprite art comes in a later phase.)
---------------------------------------------------------------------------------*/
#include <snes.h>

// Play grid dimensions (from TetrisPlayArea: width 9, height 12)
#define GRID_W 9
#define GRID_H 12

// Where to draw the grid on the 32x28 text console
#define GRID_X 11
#define GRID_Y 8

int main(void)
{
    unsigned char row;

    // Initialize text console with the default font
    consoleInitDefaultText(0);

    // Init background 0
    bgSetGfxPtr(0, 0x3000);
    bgSetMapPtr(0, 0x6800, SC_32x32);

    // 16 color mode; disable the other BGs
    setMode(BG_MODE1, 0);
    bgSetDisable(1);
    bgSetDisable(2);

    // HUD labels
    consoleDrawText(9, 2, "POMPOM TETRIS - SNES");
    consoleDrawText(GRID_X - 4, 5, "SCORE 000000");

    // Draw the 9x12 grid: '.' cells inside a '#' border
    consoleDrawText(GRID_X - 1, GRID_Y - 1, "###########");
    for (row = 0; row < GRID_H; row++)
    {
        consoleDrawText(GRID_X - 1, GRID_Y + row, "#.........#");
    }
    consoleDrawText(GRID_X - 1, GRID_Y + GRID_H, "###########");

    setScreenOn();

    while (1)
    {
        WaitForVBlank();
    }
    return 0;
}
