// Include FreeRTOS Libraries
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// Include xilinx Libraries
#include "xparameters.h"
#include "xgpio.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "xil_cache.h"

// Other miscellaneous libraries
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "pmodkypd.h"
#include "sleep.h"
#include "PmodOLED.h"
#include "OLEDControllerCustom.h"

#define STATE_SHOW_SEQ 0
#define STATE_INPUT 1
#define STATE_GAME_OVER 2
#define STATE_WIN_ROUND 3
#define STATE_START 4

#define BTN_DEVICE_ID  XPAR_GPIO_INPUTS_BASEADDR
#define KYPD_DEVICE_ID XPAR_GPIO_KEYPAD_BASEADDR
#define KYPD_BASE_ADDR XPAR_GPIO_KEYPAD_BASEADDR
#define BTN_CHANNEL    1


#define FRAME_DELAY 50000

// keypad key table
#define DEFAULT_KEYTABLE "0FED789C456B123A"

// Declaring the devices
XGpio btnInst;
PmodOLED oledDevice;
PmodKYPD KYPDInst;

// Function prototypes
void InitializeKeypad();
void initializeScreen();
static void keypadTask( void *pvParameters );
static void oledTask( void *pvParameters );
static void buttonTask( void *pvParameters );
int grphClampXco(int xco);
int grphClampYco(int yco);
int grphAbs(int foo);
void OLED_DrawLineTo(PmodOLED *InstancePtr, int xco, int yco);
void OLED_getPos(PmodOLED *InstancePtr, int *pxco, int *pyco);
void drawTarget(u8 targetX, u8 targetY, u8 width, u8 length);

u8 sequence[20]; //for storing the session sequence itself
u8 ssd_digits[9] { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };

