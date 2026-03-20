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
#include <projdefs.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <xstatus.h>
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
#define SSD_DEVICE_ID XPAR_GPIO_SSD_BASEADDR
#define BTN_CHANNEL    1


#define FRAME_DELAY 50000

// keypad key table
#define DEFAULT_KEYTABLE "0FED789C456B123A"

// Declaring the devices
XGpio btnInst;
PmodOLED oledDevice;
PmodKYPD KYPDInst;
XGpio ssdInst;

// Function prototypes
void InitializeKeypad(void);
static void keypadTask( void *pvParameters );
static void oledTask( void *pvParameters );
static void buttonTask( void *pvParameters );
void drawGrid(void);
void highlightSquare(int n);
void generateSequence(void);

// global variables

u8 sequence[20]; //for storing the session sequence itself
u8 ssd_digits[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };


int gridX[10] = {0, 2, 12, 22, 2, 12, 22, 2, 12, 22};
int gridY[10] = {0, 2, 2, 2, 12, 12, 12, 22, 22, 22};

const u8 orientation = 0x01;
const u8 invert = 0x00;

volatile u8 first_input = 0;
volatile u8 keypad_val = 0;
volatile u8 last_key = 0;
volatile u8 new_press = 0;
volatile u8 game_state = STATE_START;
volatile u8 input_index = 0;
volatile u8 current_round = 1;
volatile u8 score = 0;

typedef struct {
    int highScore; //to keep count of the score
    char myName[16]; //enter intials to track score to user
} Stats;
Stats gameStats = {0, "Aniketh"}; //hardcoded my own name into mychar

int main(void) {
    int status_buttons = 0;
    int status_ssd = 0;
    Stats gameStats = {0, "Aniketh"}; //hardcoded my own name into mychar

    //INITIALIZATIONS
    //keypad
    InitializeKeypad();

    //oled init
    OLED_Begin(&oledDevice,XPAR_GPIO_OLED_BASEADDR,XPAR_SPI_OLED_BASEADDR, orientation, invert);
   
    // Buttons
	status_buttons = XGpio_Initialize(&btnInst, BTN_DEVICE_ID);
	if(status_buttons != XST_SUCCESS){
		xil_printf("GPIO Initialization for SSD failed.\r\n");
		return XST_FAILURE;
	}

    //ssd gpio
    status_ssd = XGpio_Initialize(&ssdInst, SSD_DEVICE_ID);
    if (status_ssd != XST_SUCCESS) {
        xil_printf("SSD Initialization failed.\r\n");
        return XST_FAILURE;
    }
    XGpio_SetDataDirection(&ssdInst,BTN_CHANNEL, 0);


    	xil_printf("Initialization Complete, System Ready!\n");

    //random seed -- will always call the same random sequence
    srand(100);

	xTaskCreate( keypadTask					/* The function that implements the task. */
			   , "keypad task"				/* Text name for the task, provided to assist debugging only. */
			   , configMINIMAL_STACK_SIZE	/* The stack allocated to the task. */
			   , NULL						/* The task parameter is not used, so set to NULL. */
			   , tskIDLE_PRIORITY			/* The task runs at the idle priority. */
			   , NULL
			   );


	xTaskCreate( oledTask					/* The function that implements the task. */
			   , "screen task"				/* Text name for the task, provided to assist debugging only. */
			   , configMINIMAL_STACK_SIZE	/* The stack allocated to the task. */
			   , NULL						/* The task parameter is not used, so set to NULL. */
			   , tskIDLE_PRIORITY			/* The task runs at the idle priority. */
			   , NULL
			   );

	xTaskCreate( buttonTask
			   , "button task"
			   , configMINIMAL_STACK_SIZE
			   , NULL
			   , tskIDLE_PRIORITY
			   , NULL
			   );

    
    vTaskStartScheduler();
    while (1);
    return 0;

}

void InitializeKeypad()
{
    KYPD_begin(&KYPDInst, KYPD_BASE_ADDR);
    KYPD_loadKeyTable(&KYPDInst, (u8*) DEFAULT_KEYTABLE);
}


void drawGrid() {
    for (int i = 1; i <= 9; i++) {
        OLED_MoveTo(&oledDevice, gridX[i], gridY[i]);
        OLED_RectangleTo(&oledDevice, gridX[i] +7, gridY[i]+7);
    }

}

void highlightSquare(int n) {
    OLED_SetDrawColor(&oledDevice, 1);
    OLED_MoveTo(&oledDevice, gridX[n], gridY[n]);
    OLED_FillRect(&oledDevice, gridX[n]+7, gridY[n]+7);
}


void generateSequence() {
    for  (int i = 0; i < 20; i++) {
        sequence[i] = (rand()%9) + 1;
    }
}

static void keypadTask( void *pvParameters )
{
   u16 keystate;
   u8 flag = 0;
   XStatus status, last_status = KYPD_NO_KEY;
   u8 new_key = 'x';

   const TickType_t xDelay = pdMS_TO_TICKS(40);
   
   xil_printf("Pmod KYPD app started. Press any key on the Keypad.\r\n");
   while (1) {
	  // Capture state of the keypad
	  keystate = KYPD_getKeyStates(&KYPDInst);

	  // Determine which single key is pressed, if any
	  // if a key is pressed, store the value of the new key in new_key
	  status = KYPD_getKeyPressed(&KYPDInst, keystate, &new_key);

	  // Print key detect if a new key is pressed or if status has changed
	  if (status == KYPD_SINGLE_KEY){
          if(last_status != KYPD_SINGLE_KEY){
              keypad_val = new_key;
              new_press = 1;
          }
	  } else if (status == KYPD_MULTI_KEY && status != last_status){
		 xil_printf("Error: Multiple keys pressed\r\n");
	  } 

	  last_status = status;	  
	  vTaskDelay(xDelay); // Scanning Delay
   }
}

//provided the implementation for oled task
static void oledTask(void *pvParameters) {
    char temp[20];
    int i;
    OLED_SetDrawColor(&oledDevice, 1);
    OLED_SetDrawMode(&oledDevice, 0);
    OLED_SetCharUpdate(&oledDevice, 0);

    while (1) {
        if (game_state == STATE_START) {
            OLED_ClearBuffer(&oledDevice);
            OLED_SetCursor(&oledDevice, 0, 0);
            OLED_PutString(&oledDevice, "Memory Game");
            OLED_SetCursor(&oledDevice,0, 2);
            OLED_PutString(&oledDevice, "Press BTN0");
            OLED_Update(&oledDevice);

            u8 btnVal = XGpio_DiscreteRead(&btnInst, BTN_CHANNEL);

            if (btnVal == 1) {
                generateSequence();
                current_round = 1;
                score = 0;
                input_index = 0;
                game_state = STATE_SHOW_SEQ;
            }

        }

		else if (game_state == STATE_SHOW_SEQ) {
		    // update SSD with current level
		    XGpio_DiscreteWrite(&ssdInst, 1, ssd_digits[current_round] | (1 << 8));
		
		    // shows each square in the sequence
		    for (i = 0; i < current_round; i++) {
		        // shows grid with highlighted square
		        OLED_ClearBuffer(&oledDevice);
		        drawGrid();
		        highlightSquare(sequence[i]);
		        OLED_Update(&oledDevice);
		        vTaskDelay(100);
		
		        // brief gap between squares
		        OLED_ClearBuffer(&oledDevice);
		        drawGrid();
		        OLED_Update(&oledDevice);
		        vTaskDelay(40);
		    }
		
		    // done showing, switch to input mode
		    new_press = 0;
            keypad_val = 0;
            first_input = 1;
		    game_state = STATE_INPUT;
		}


        else if (game_state == STATE_INPUT) {
		    OLED_ClearBuffer(&oledDevice);
		    drawGrid();

            if (first_input) {
                OLED_ClearBuffer(&oledDevice);
		        drawGrid();
                OLED_SetCursor(&oledDevice, 5, 1);
                OLED_PutString(&oledDevice, "Your turn");
                OLED_Update(&oledDevice);
                vTaskDelay(100);
                first_input = 0;
            }
		
		    OLED_ClearBuffer(&oledDevice);
            drawGrid();
            OLED_Update(&oledDevice);
		
		    if (new_press) {
		        new_press = 0;
		        u8 pressed = keypad_val - '0';
		
		        if (pressed >= 1 && pressed <= 9) {
		            if (pressed == sequence[input_index]) {
		                // correct
		                input_index++;
		
		                // show the square they pressed
		                OLED_ClearBuffer(&oledDevice);
		                drawGrid();
		                highlightSquare(pressed);
		                OLED_Update(&oledDevice);
		                vTaskDelay(100);
		
		                if (input_index == current_round) {
		                    game_state = STATE_WIN_ROUND;
		                }
		            } else {
		                // wrong
		                game_state = STATE_GAME_OVER;
		            }
		        }
		    }
		}

        else if (game_state == STATE_WIN_ROUND) {
		    OLED_ClearBuffer(&oledDevice);
		    OLED_SetCursor(&oledDevice, 0, 1);
		    OLED_PutString(&oledDevice, "Correct!");
		    OLED_Update(&oledDevice);
		    vTaskDelay(100);
		
		    score++;
		    current_round++;
		    input_index = 0;
		    game_state = STATE_SHOW_SEQ;
		}

        else if (game_state == STATE_GAME_OVER) {
		    OLED_ClearBuffer(&oledDevice);
		    OLED_SetCursor(&oledDevice, 0, 0);
		    OLED_PutString(&oledDevice, "Game Over!");
		    OLED_SetCursor(&oledDevice, 0, 2);
		    char temp[16];
		    sprintf(temp, "Score: %d", score);
		    OLED_PutString(&oledDevice, temp);
		    OLED_SetCursor(&oledDevice, 0, 3);
		    OLED_PutString(&oledDevice, "BTN0: Restart");
		    OLED_Update(&oledDevice);
		
		    u8 btnVal = XGpio_DiscreteRead(&btnInst, BTN_CHANNEL);
		    if (btnVal == 1) {
		        game_state = STATE_START;
		    }

            if (score > gameStats.highScore) {
                gameStats.highScore = score;
            }

            sprintf(temp, "High: %d", gameStats.highScore);
            OLED_PutString(&oledDevice, temp);
		}
	}    
}

//btn 0 starts / restarts the game 
static void buttonTask( void *pvParameters) {
    u8 btnVal = 0;
    while (1) {
        btnVal = XGpio_DiscreteRead(&btnInst, BTN_CHANNEL);
        if (btnVal == 8) {
            game_state = STATE_START;
        }
        vTaskDelay(50);
    }
}


