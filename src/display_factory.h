#pragma once
#include "display_hal.h"
#include "types.h"

// Creates the correct IDisplay implementation for the given board target.
// For Board A, panelVariant selects the GxEPD2 driver at runtime (PRD §6.4).
IDisplay* createDisplay(BoardTarget target, PanelVariant panelVariant);
