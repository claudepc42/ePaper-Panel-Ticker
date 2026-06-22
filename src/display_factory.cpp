#include "display_factory.h"
#include "hal/display_board_a.h"
#include "hal/display_board_b.h"
#include "hal/display_board_c.h"

// Use compile-time flag to pick the display class. Each board build ships a
// single binary for one board, so BOARD_BUILD_* is the authoritative source.
// cfg.boardTarget / panelVariant are stored for informational / portal UI use
// and as the runtime variant selector on Board A (Panel A vs B).
IDisplay* createDisplay(BoardTarget /*target*/, PanelVariant panelVariant) {
#if defined(BOARD_BUILD_B)
    (void)panelVariant;
    return new DisplayBoardB();
#elif defined(BOARD_BUILD_C)
    (void)panelVariant;
    return new DisplayBoardC();
#else
    // Board A: panelVariant selects the GxEPD2 driver (PRD §6.4)
    return new DisplayBoardA(panelVariant);
#endif
}
