#include "FreeRTOS.h"
#include "PmodOLED.c"
#include "pmodkypd.c"
#include "OledGrph.c"
#include "OLEDControllerCustom.h"
#include <stdlib.h>


#define STATE_SHOW_SEQ 0
#define STATE_INPUT 1
#define STATE_GAME_OVER 2
#define STATE_WIN_ROUND 2
#define STATE_START 3

u8 sequence[20];

