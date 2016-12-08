/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/
#include <MKL25Z4.H>
#include <stdio.h>
#include "gpio_defs.h"
#include <string.h>
#include "tft_lcd.h"
#include "font.h"

#include "LEDs.h"
#include "timers.h"
#include "sound.h"
#include "DMA.h"

#include "I2C.h"
#include "mma8451.h"
#include "delay.h"
#include "profile.h"
#include "math.h"

#include <RTL.h>
#include "tasks.h"
#include "snake.h"

#define USE_GFX_LCD


/*----------------------------------------------------------------------------
  MAIN function
 *----------------------------------------------------------------------------*/
int main (void) {
	Init_Debug_Signals();
//	Init_RGB_LEDs();
	Sound_Init();	
	i2c_init();		
	if (!init_mma()) {							// init accelerometer
//		Control_RGB_LEDs(1,0,0);			// accel initialization failed, so turn on red error light
		while (1)
			;
	}
	// Sound_Disable_Amp();
	Play_Tone();
	//todo init pit for cpu utilization
	
	
	
	TFT_Init();
	TFT_Text_Init(1);
	TFT_Erase();
//	TFT_Text_PrintStr_RC(0,0, "Test Code");
//	 DrawFilledCircle(3,3, 2, red);
//		intro();
//		resetGame();
//		while(1)
//		{
//			snake();
//		}
		
	//TFT_TS_Calibrate();
//	TFT_TS_Test();

//	TFT_Text_PrintStr_RC(1,0, "Accel...");

//	i2c_init();											// init I2C peripheral
//	if (!init_mma()) {							// init accelerometer
//		Control_RGB_LEDs(1,0,0);			// accel initialization failed, so turn on red error light
//		while (1)
//			;
//	}
//	TFT_Text_PrintStr_RC(1,9, "Done");

	Delay(70);

	os_sys_init(&Task_Init);

	while (1)
		;
}

// *******************************ARM University Program Copyright © ARM Ltd 2013*************************************   
