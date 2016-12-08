/* Code for configuring and controlling TFT LCD using ILI9341 controller. */

#include <stdint.h>
#include <stdio.h>
#include "MKL25Z4.h"
#include "TFT_LCD.h"
#include "gpio_defs.h"
#include "timers.h"
#include "font.h"
#include "tasks.h"


extern void Delay(uint32_t);
extern bool os_is_ready;

/* Is set to one if touchscreen been calibrated. */
uint8_t TFT_TS_Calibrated = 1;
uint32_t TFT_TS_X_Scale=209, TFT_TS_X_Offset=6648;
uint32_t TFT_TS_Y_Scale=159, TFT_TS_Y_Offset=6689;

PT_T TS_Min;
PT_T TS_Max;

/* Initialize GPIO ports for communicating with TFT LCD controller. */
void TFT_GPIO_Init(void) {
	unsigned i;
	
	// Enable clock to ports
	SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK | SIM_SCGC5_PORTE_MASK;
	
	// Make digital pins GPIO
	// Data bus pins
	for (i=TFT_DB8_POS; i<=TFT_DB15_POS; i++) {
		PORTC->PCR[i] &= ~PORT_PCR_MUX_MASK;
		PORTC->PCR[i] |= PORT_PCR_MUX(1);
		PTC->PDDR |= MASK(i);
	}
	
	// Data/Command
	PORTC->PCR[TFT_D_NC_POS] &= ~PORT_PCR_MUX_MASK;
	PORTC->PCR[TFT_D_NC_POS] |= PORT_PCR_MUX(1);
	// /Write
	PORTC->PCR[TFT_NWR_POS] &= ~PORT_PCR_MUX_MASK;
	PORTC->PCR[TFT_NWR_POS] |= PORT_PCR_MUX(1);
	// /Read
	PORTC->PCR[TFT_NRD_POS] &= ~PORT_PCR_MUX_MASK;
	PORTC->PCR[TFT_NRD_POS] |= PORT_PCR_MUX(1);
	// /Reset
	PORTC->PCR[TFT_NRST_POS] &= ~PORT_PCR_MUX_MASK;
	PORTC->PCR[TFT_NRST_POS] |= PORT_PCR_MUX(1);

	// Set port directions to outputs
	FPTC->PDDR |= MASK(TFT_D_NC_POS);
	FPTC->PDDR |= MASK(TFT_NWR_POS);
	FPTC->PDDR |= MASK(TFT_NRD_POS);
	FPTC->PDDR |= MASK(TFT_NRST_POS);

	// Set initial control line values to inactive (1)
	FPTC->PDOR |= MASK(TFT_D_NC_POS);
	FPTC->PDOR |= MASK(TFT_NWR_POS);
	FPTC->PDOR |= MASK(TFT_NRD_POS);
	FPTC->PDOR |= MASK(TFT_NRST_POS);
}

 void Old_Init_PWM(void);

/* Initialize hardware for LCD backlight control and set to default value. */
void TFT_Init_Backlight(void) {
	// Init_PWM(TFT_BL_TPM, TFT_BL_TPM_CHANNEL, 10000 /* TFT_BL_PERIOD*/, 5000 /* TFT_BL_PERIOD/2*/);	
	Old_Init_PWM();
	// TFT_Set_Backlight_Brightness(100);
}

/* Set backlight brightness for LCD via duty cycle. Function is currently broken. */
void TFT_Set_Backlight_Brightness(uint16_t brightness_percent){
	PWM_Set_Value(TFT_BL_TPM, TFT_BL_TPM_CHANNEL, (brightness_percent*100)/TFT_BL_PERIOD);
}


/* Write one byte as a command to the TFT LCD controller. */
void TFT_24S_Write_Command(uint8_t command)
{
	
	if(os_is_ready){
		get_stack_depth(MAX_STACK);
	}
	GPIO_ResetBit(TFT_D_NC_POS);
	GPIO_Write(command);
	GPIO_ResetBit(TFT_NWR_POS);
	GPIO_SetBit(TFT_NWR_POS);
}

/* Write one byte as data to the TFT LCD Controller. */
void TFT_24S_Write_Data(uint8_t data)
{
	GPIO_SetBit(TFT_D_NC_POS);
	GPIO_Write(data);
	GPIO_ResetBit(TFT_NWR_POS);
	GPIO_SetBit(TFT_NWR_POS);
}

/* Initialize the relevant peripherals (GPIO, TPM, ADC) and the display
components (TFT LCD controller, touch screen and backlight controller). */ 
void TFT_Init()
{
	TFT_GPIO_Init();
	TFT_TS_Init();
	TFT_Init_Backlight();
	
	GPIO_SetBit(TFT_NRD_POS);
	GPIO_ResetBit(TFT_NWR_POS);
	GPIO_ResetBit(TFT_NRST_POS);
	Delay(100);
	GPIO_SetBit(TFT_NRST_POS);
	Delay(200);
	TFT_24S_Write_Command(0x0028); //display OFF
	TFT_24S_Write_Command(0x0011); //exit SLEEP mode
	TFT_24S_Write_Data(0x0000);
	TFT_24S_Write_Command(0x00CB); //Power Control A
	TFT_24S_Write_Data(0x0039); //always 0x39
	TFT_24S_Write_Data(0x002C); //always 0x2C
	TFT_24S_Write_Data(0x0000); //always 0x00
	TFT_24S_Write_Data(0x0034); //Vcore = 1.6V
	TFT_24S_Write_Data(0x0002); //DDVDH = 5.6V
	TFT_24S_Write_Command(0x00CF); //Power Control B
	TFT_24S_Write_Data(0x0000); //always 0x00
	TFT_24S_Write_Data(0x0081); //PCEQ off
	TFT_24S_Write_Data(0x0030); //ESD protection
	TFT_24S_Write_Command(0x00E8); //Driver timing control A
	TFT_24S_Write_Data(0x0085); //non-overlap
	TFT_24S_Write_Data(0x0001); //EQ timing
	TFT_24S_Write_Data(0x0079); //Pre-charge timing
	TFT_24S_Write_Command(0x00EA); //Driver timing control B
	TFT_24S_Write_Data(0x0000); //Gate driver timing
	TFT_24S_Write_Data(0x0000); //always 0x00
	TFT_24S_Write_Command(0x00ED); //Power-On sequence control
	TFT_24S_Write_Data(0x0064); //soft start
	TFT_24S_Write_Data(0x0003); //power on sequence
	TFT_24S_Write_Data(0x0012); //power on sequence
	TFT_24S_Write_Data(0x0081); //DDVDH enhance on
	TFT_24S_Write_Command(0x00F7); //Pump ratio control
	TFT_24S_Write_Data(0x0020); //DDVDH=2xVCI
	TFT_24S_Write_Command(0x00C0); //power control 1
	TFT_24S_Write_Data(0x0026);
	TFT_24S_Write_Data(0x0004); //second parameter for ILI9340 (ignored by ILI9341)
	TFT_24S_Write_Command(0x00C1); //power control 2
	TFT_24S_Write_Data(0x0011);
	TFT_24S_Write_Command(0x00C5); //VCOM control 1
	TFT_24S_Write_Data(0x0035);
	TFT_24S_Write_Data(0x003E);
	TFT_24S_Write_Command(0x00C7); //VCOM control 2
	TFT_24S_Write_Data(0x00BE);
	TFT_24S_Write_Command(0x0036); //memory access control = BGR
#if 1
	TFT_24S_Write_Data(0x0008); // Portrait mode
#else
	TFT_24S_Write_Data(0x00A8); // Landscape mode
#endif
	TFT_24S_Write_Command(0x00B1); //frame rate control
	TFT_24S_Write_Data(0x0000);
	TFT_24S_Write_Data(0x0010);
	TFT_24S_Write_Command(0x00B6); //display function control
	TFT_24S_Write_Data(0x000A);
	TFT_24S_Write_Data(0x00A2);
	TFT_24S_Write_Command(0x003A); //pixel format = 16 bit per pixel
	TFT_24S_Write_Data(0x0055);
	TFT_24S_Write_Command(0x00F2); //3G Gamma control
	TFT_24S_Write_Data(0x0002); //off
	TFT_24S_Write_Command(0x0026); //Gamma curve 3
	TFT_24S_Write_Data(0x0001);
	TFT_24S_Write_Command(0x002A); //column address set
	TFT_24S_Write_Data(0x0000);
	TFT_24S_Write_Data(0x0000); //start 0x0000
	TFT_24S_Write_Data(0x0000);
	TFT_24S_Write_Data(0x00EF); //end 0x00EF
	TFT_24S_Write_Command(0x002B); //page address set
	TFT_24S_Write_Data(0x0000);
	TFT_24S_Write_Data(0x0000); //start 0x0000
	TFT_24S_Write_Data(0x0001);
	TFT_24S_Write_Data(0x003F); //end 0x013F
	TFT_24S_Write_Command(0x0029); //Display ON
}

/* Set the pixel at pos to the given color. */
void TFT_Plot_Pixel(PT_T * pos, COLOR_T * color) {
	uint8_t b1, b2;

	// Column address set 0x2a
	TFT_24S_Write_Command(0x002A); //column address set
	TFT_24S_Write_Data(0);
	TFT_24S_Write_Data(pos->X & 0xff); //start 
	TFT_24S_Write_Data(0x0000);
	TFT_24S_Write_Data(0x00EF); //end 0x00EF
	
	// Page (row) address set 0x2b
	TFT_24S_Write_Command(0x002B); //page address set
	TFT_24S_Write_Data(pos->Y >> 8);
	TFT_24S_Write_Data(pos->Y & 0xff); //start 
	TFT_24S_Write_Data(0x0001);
	TFT_24S_Write_Data(0x003F); //end 0x013F	
	
	// Memory Write 0x2c
	// 16 bpp, 5-6-5. Assume color channel data is left-aligned
	b1 = (color->R&0xf8) | ((color->G&0xe0)>>5);
	b2 = ((color->G&0x1c)<<3) | ((color->B&0xf8)>>3);

	TFT_24S_Write_Command(0x002c);
	TFT_24S_Write_Data(b1);
	TFT_24S_Write_Data(b2);
}

/* Fill the entire display buffer with the given color. */
void TFT_Fill_Buffer(COLOR_T * color) {
	uint32_t x, y;
	uint8_t b1, b2;
	
	// Enable access to full screen, reset write pointer to origin
	TFT_24S_Write_Command(0x002A); //column address set
	TFT_24S_Write_Data(0x0000);
	TFT_24S_Write_Data(0x0000); //start 0x0000
	TFT_24S_Write_Data(0x0000);
	TFT_24S_Write_Data(0x00EF); //end 0x00EF
	TFT_24S_Write_Command(0x002B); //page address set
	TFT_24S_Write_Data(0x0000);
	TFT_24S_Write_Data(0x0000); //start 0x0000
	TFT_24S_Write_Data(0x0001);
	TFT_24S_Write_Data(0x003F); //end 0x013F
	
	// Memory Write 0x2c
	// 16 bpp, 5-6-5. Assume color channel data is left-aligned
	b1 = (color->R&0xf8) | ((color->G&0xe0)>>5);
	b2 = ((color->G&0x1c)<<3) | ((color->B&0xf8)>>3);
	
	TFT_24S_Write_Command(0x002c);
	for (y=0; y<320; y++) {
		for (x=0; x<240; x++) {
			TFT_24S_Write_Data(b1);
			TFT_24S_Write_Data(b2);
		}
	}		
}
/* Draw a rectangle from p1 to p2 filled with specified color. */
void TFT_Fill_Rectangle(PT_T * p1, PT_T * p2, COLOR_T * color) {
	uint32_t n;
	uint8_t b1, b2;
	uint16_t c_min, c_max, r_min, r_max;
	
	c_min = MIN(p1->X, p2->X);
	c_max = MAX(p1->X, p2->X);
	c_max = MIN(c_max, TFT_WIDTH-1);
	
	r_min = MIN(p1->Y, p2->Y);
	r_max = MAX(p1->Y, p2->Y);
	r_max = MIN(r_max, TFT_HEIGHT-1);

	n = (c_max - c_min + 1)*(r_max - r_min + 1);
	if (n == 0)
		return;
	
	// Enable access to full screen, reset write pointer to origin
	TFT_24S_Write_Command(0x002A); //column address set
	TFT_24S_Write_Data(c_min >> 8);
	TFT_24S_Write_Data(c_min & 0xff); //start 
	TFT_24S_Write_Data(c_max >> 8);
	TFT_24S_Write_Data(c_max & 0xff); //end 
	TFT_24S_Write_Command(0x002B); //page address set
	TFT_24S_Write_Data(r_min >> 8);
	TFT_24S_Write_Data(r_min & 0xff); //start 
	TFT_24S_Write_Data(r_max >> 8);
	TFT_24S_Write_Data(r_max & 0xff); //end 
	
	// Memory Write 0x2c
	// 16 bpp, 5-6-5. Assume color channel data is left-aligned
	b1 = (color->R&0xf8) | ((color->G&0xe0)>>5);
	b2 = ((color->G&0x1c)<<3) | ((color->B&0xf8)>>3);
	
	TFT_24S_Write_Command(0x002c);
	
	while (n-- > 0) {
		TFT_24S_Write_Data(b1);
		TFT_24S_Write_Data(b2);
	}	
}

/* Prepare LCD controller draw rectangle from p1 to p2 using future pixels provided 
by calls to TFT_Write_Rectangle_pixel. 
Return value is number of pixels expected. 
*/
uint32_t TFT_Start_Rectangle(PT_T * p1, PT_T * p2) {
	uint32_t n;
	uint16_t c_min, c_max, r_min, r_max;
	
	// Find bounds of rectangle
	c_min = MIN(p1->X, p2->X);
	c_max = MAX(p1->X, p2->X);
	
	r_min = MIN(p1->Y, p2->Y);
	r_max = MAX(p1->Y, p2->Y);

	// Clip to display size
	c_max = MIN(c_max, TFT_WIDTH-1);
	r_max = MIN(r_max, TFT_HEIGHT-1);
	
	n = (c_max - c_min + 1)*(r_max - r_min + 1);
	if (n > 0) {
		// Enable access to full screen, reset write pointer to origin
		TFT_24S_Write_Command(0x002A); //column address set
		TFT_24S_Write_Data(c_min >> 8);
		TFT_24S_Write_Data(c_min & 0xff); //start 
		TFT_24S_Write_Data(c_max >> 8);
		TFT_24S_Write_Data(c_max & 0xff); //end 
		TFT_24S_Write_Command(0x002B); //page address set
		TFT_24S_Write_Data(r_min >> 8);
		TFT_24S_Write_Data(r_min & 0xff); //start 
		TFT_24S_Write_Data(r_max >> 8);
		TFT_24S_Write_Data(r_max & 0xff); //end 
		
		// Memory Write 0x2c
		TFT_24S_Write_Command(0x002c);
	}	
	return n;
}

/* Plot this pixel in the next location as defined by TFT_Start_Rectangle. You must 
have called TFT_Write_Rectangle before calling this function. */
void TFT_Write_Rectangle_Pixel(COLOR_T * color, unsigned int count) {
	uint8_t b1, b2;

	// 16 bpp, 5-6-5. Assume color channel data is left-aligned
	b1 = (color->R&0xf8) | ((color->G&0xe0)>>5);
	b2 = ((color->G&0x1c)<<3) | ((color->B&0xf8)>>3);
	while (count--) {
		TFT_24S_Write_Data(b1);
		TFT_24S_Write_Data(b2);
	}
}

/* Enable (on > 0) or disable LED backlight via LCD controller. */
void TFT_Set_BL(uint8_t on) {
	TFT_24S_Write_Command(0x53);
	TFT_24S_Write_Data(0x28 | (on? 4 : 0));
}

/* Set brightness via LCD controller. */
void TFT_Set_Brightness(uint8_t brightness) {
	TFT_24S_Write_Command(0x51);
	TFT_24S_Write_Data(brightness);
}

void Init_ADC(void) {
	
	SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK; 
	ADC0->CFG1 = 0x9C; // 16 bit
	ADC0->SC2 = 0;
}


/* Initialize touchscreen hardware (ADC). */
void TFT_TS_Init(void) {
	// Configure ADC
	Init_ADC();
}


static uint16_t xl=0, yu=0;

/* Read touch screen. Returns 1 if touched, and updates position. Else returns 0 leaving 
position unchanged. */
uint32_t TFT_TS_Read(PT_T * position) {
	uint32_t x, y;
	uint32_t b;
	
	get_stack_depth(MAX_STACK);
	
	// Determine if screen was pressed.
	// Set YU digital output at ground, 
	PORTE->PCR[TFT_TS_YU_BIT] &= ~PORT_PCR_MUX_MASK;
	PORTE->PCR[TFT_TS_YU_BIT] |= PORT_PCR_MUX(1);
	PTE->PDDR |= MASK(TFT_TS_YU_BIT);
	PTE->PCOR = MASK(TFT_TS_YU_BIT); // Clear YU to 0

	// Set YD + XL to open
	// Set YD as ADC input
	PORTE->PCR[TFT_TS_YD_BIT] &= ~PORT_PCR_MUX_MASK;
	PORTE->PCR[TFT_TS_YD_BIT] |= PORT_PCR_MUX(0);

	// Set XL as digital input
	PORTE->PCR[TFT_TS_XL_BIT] &= ~PORT_PCR_MUX_MASK;
	PORTE->PCR[TFT_TS_XL_BIT] |= PORT_PCR_MUX(1);
	PTE->PDDR &= ~MASK(TFT_TS_XL_BIT); // Input
	
	// Set XR as digital input with pull-up
	PORTE->PCR[TFT_TS_XR_BIT] &= ~PORT_PCR_MUX_MASK;
	PORTE->PCR[TFT_TS_XR_BIT] |= PORT_PCR_MUX(1);
	PORTE->PCR[TFT_TS_XR_BIT] |= PORT_PCR_PE_MASK; // Enable pull-up
	PTE->PDDR &= ~MASK(TFT_TS_XR_BIT); // Input
		
	// Wait for the inputs to settle
	Delay(TS_DELAY);
	// Read XR input via digital
	// if XR is 0, then screen is pressed
	b = (PTE->PDIR) & MASK(TFT_TS_XR_BIT);
	
	if (b>0) {
		// Screen not pressed
		return 0;
	} else {
		// Read X Position
		// Configure inputs to ADC
		PORTE->PCR[TFT_TS_YU_BIT] &= ~PORT_PCR_MUX_MASK;
		PORTE->PCR[TFT_TS_YU_BIT] |= PORT_PCR_MUX(0);
		PORTE->PCR[TFT_TS_YD_BIT] &= ~PORT_PCR_MUX_MASK;
		PORTE->PCR[TFT_TS_YD_BIT] |= PORT_PCR_MUX(0);

		
		// Configure outputs to GPIO
		PORTE->PCR[TFT_TS_XL_BIT] &= ~PORT_PCR_MUX_MASK;
		PORTE->PCR[TFT_TS_XL_BIT] |= PORT_PCR_MUX(1);
		PORTE->PCR[TFT_TS_XR_BIT] &= ~PORT_PCR_MUX_MASK;
		PORTE->PCR[TFT_TS_XR_BIT] |= PORT_PCR_MUX(1);
		PTE->PDDR |= MASK(TFT_TS_XL_BIT) | MASK(TFT_TS_XR_BIT);
		PTE->PSOR = MASK(TFT_TS_XR_BIT); // Set XR to 1
		PTE->PCOR = MASK(TFT_TS_XL_BIT); // Clear XL to 0
		// Wait for inputs to settle
		Delay(TS_DELAY);
		
		// Read inputs
		ADC0->SC1[0] = TFT_TS_YU_CHANNEL; // start conversion on channel YU
		while (!(ADC0->SC1[0] & ADC_SC1_COCO_MASK))
			;
		yu = ADC0->R[0];
		x = yu;

		// Read Y Position
		// Configure inputs to ADC
		PORTE->PCR[TFT_TS_XL_BIT] &= ~PORT_PCR_MUX_MASK;
		PORTE->PCR[TFT_TS_XL_BIT] |= PORT_PCR_MUX(0);
		PORTE->PCR[TFT_TS_XR_BIT] &= ~PORT_PCR_MUX_MASK;
		PORTE->PCR[TFT_TS_XR_BIT] |= PORT_PCR_MUX(0);
		// Disable pull-up - just to be sure
		PORTE->PCR[TFT_TS_XR_BIT] &= ~PORT_PCR_PE_MASK; 
		
		// Configure outputs to GPIO
		PORTE->PCR[TFT_TS_YU_BIT] &= ~PORT_PCR_MUX_MASK;
		PORTE->PCR[TFT_TS_YU_BIT] |= PORT_PCR_MUX(1);
		PORTE->PCR[TFT_TS_YD_BIT] &= ~PORT_PCR_MUX_MASK;
		PORTE->PCR[TFT_TS_YD_BIT] |= PORT_PCR_MUX(1);
		PTE->PDDR |= MASK(TFT_TS_YU_BIT) | MASK(TFT_TS_YD_BIT);
		PTE->PSOR = MASK(TFT_TS_YD_BIT); // Set YD to 1
		PTE->PCOR = MASK(TFT_TS_YU_BIT); // Clear YU to 0
		// Wait for the inputs to settle
		Delay(TS_DELAY);

		// Read inputs
		ADC0->SC1[0] = TFT_TS_XL_CHANNEL; // start conversion on channel XL
		while (!(ADC0->SC1[0] & ADC_SC1_COCO_MASK))
			;
		xl = ADC0->R[0];
		y = xl;

		// Apply calibration factors to raw position information
		if (TFT_TS_Calibrated) {
			if (x<TFT_TS_X_Offset) {
				position->X = 0;
			} else {
				position->X = (x - TFT_TS_X_Offset)/TFT_TS_X_Scale;
			}
			if (y<TFT_TS_Y_Offset) {
				position->Y = 0;
			} else {
				position->Y = (y - TFT_TS_Y_Offset)/TFT_TS_Y_Scale;
			}
		} else {
			position->X = x;
			position->Y = y;
		}
		return 1;
	}
}

/* Test touchscreen by printing touch coordinates and drawing lines where touched. */
void TFT_TS_Test(void) {
	PT_T p, bp, pp;
	COLOR_T c;
	char buffer[32];
	
	TFT_Erase();

	pp.X = 0;
	pp.Y = 0;
	
	bp.X = COL_TO_X(0);
	bp.Y = ROW_TO_Y(0);
	c.R = 255;
	c.G = 100;
	c.B = 100;
	
	while (1) {
		if (TFT_TS_Read(&p)) {
			bp.X = 0;
			sprintf(buffer, "X:%5d Y:%5d", p.X, p.Y);
			TFT_Text_PrintStr(&bp, buffer);

#if 0
			TFT_Plot_Pixel(&p, &c);
			p.X++;
			TFT_Plot_Pixel(&p, &c);
			p.X-=2;
			TFT_Plot_Pixel(&p, &c);
			p.X++;
			
			p.Y++;
			TFT_Plot_Pixel(&p, &c);
			p.Y-=2;
			TFT_Plot_Pixel(&p, &c);
#else
			if ((pp.X == 0) && (pp.Y == 0)) {
				pp = p;
			}
			TFT_Draw_Line(&p, &pp, &c);
			pp = p;
#endif
		} else {
			pp.X = 0;
			pp.Y = 0;
	
			bp.X = 0;
			TFT_Text_PrintStr(&bp, "Not Pressed    ");
		}
	}
}

/* Calibrate touchscreen. */
void TFT_TS_Calibrate(void) {
	PT_T p, bp, p_bound;
	uint32_t i;
	
	bp.X = COL_TO_X(0);
	bp.Y = ROW_TO_Y(3);

	TFT_Text_PrintStr(&bp, "Calibrate TS");
	bp.X = COL_TO_X(0);
	bp.Y = ROW_TO_Y(4);
	TFT_Text_PrintStr(&bp, "by pressing each +");
	
	bp.X = COL_TO_X(0);
	bp.Y = ROW_TO_Y(0);
	TFT_Text_PrintStr(&bp, "+");
	
	p_bound.X = 0xffff;
	p_bound.Y = 0xffff;
	for (i=0; i<TS_CALIB_SAMPLES; i++){
		while (!TFT_TS_Read(&p))
			;
		if (p.X < p_bound.X)
			p_bound.X = p.X;
		if (p.Y < p_bound.Y)
			p_bound.Y = p.Y;
	}
	
	TFT_TS_X_Offset = p_bound.X;
	TFT_TS_Y_Offset = p_bound.Y;
	
	TS_Min = p_bound;
	
	// Wait for release
	while (TFT_TS_Read(&p))
		;
	
	TFT_Erase();

	/*
	bp.X = COL_TO_X(TFT_MAX_COLS-1);
	bp.Y = ROW_TO_Y(TFT_MAX_ROWS-1);
	*/
	bp.X = TFT_WIDTH - CHAR_WIDTH - 1;
	bp.Y = TFT_HEIGHT - CHAR_HEIGHT - 1;
	TFT_Text_PrintStr(&bp, "+");

	p_bound.X = 0;
	p_bound.Y = 0;
	for (i=0; i<TS_CALIB_SAMPLES; i++){
		while (!TFT_TS_Read(&p))
			;
		if (p.X > p_bound.X)
			p_bound.X = p.X;
		if (p.Y > p_bound.Y)
			p_bound.Y = p.Y;
	}
	
	TS_Max = p_bound;
	
	TFT_TS_X_Scale = (p_bound.X-TFT_TS_X_Offset)/TFT_WIDTH;
	TFT_TS_Y_Scale = (p_bound.Y-TFT_TS_Y_Offset)/TFT_HEIGHT;

	// Wait for release
	while (TFT_TS_Read(&p))
		;
	
	TFT_TS_Calibrated = 1;

}
/*drawing functions*/
void DrawFilledRectangle(int p1_x, int p1_y, int p2_x, int p2_y, COLOR_T color){

	PT_T p1,p2;
	p1.X = p1_x;p1.Y = p1_y;
	p2.X = p2_x; p2.Y = p2_y;
	TFT_Fill_Rectangle(&p1,&p2,&color);
}

void DrawFilledCircle(int x, int y, int radius, COLOR_T color){
	PT_T p1,p2;
	signed int a, b, P;
  a = 0;
  b = radius;
  P = 1 - radius;

   do
   {
  //    if(fill)
   //   {
					p1.X = x-a;p1.Y=y+b;p2.X=x+a;p2.Y=y+b;
         TFT_Draw_Line(&p1, &p2, &color);
				p1.X = x-a;p1.Y=y-b;p2.X=x+a;p2.Y=y-b;
				 TFT_Draw_Line(&p1, &p2, &color);
				p1.X = x-b;p1.Y=y+a;p2.X=x+b;p2.Y=y+a;
         TFT_Draw_Line(&p1, &p2, &color);
				p1.X = x-b;p1.Y=y-a;p2.X=x+b;p2.Y=y-a;
         TFT_Draw_Line(&p1, &p2, &color);
   //   }
 /*     else
      {
         TFT_Draw_Line(a+x, b+y, color);
         TFT_Draw_Line(b+x, a+y, color);
         TFT_Draw_Line(x-a, b+y, color);
         TFT_Draw_Line(x-b, a+y, color);
         TFT_Draw_Line(b+x, y-a, color);
         TFT_Draw_Line(a+x, y-b, color);
         TFT_Draw_Line(x-a, y-b, color);
         TFT_Draw_Line(x-b, y-a, color);
      }
*/
      if(P < 0)
         P+= 3 + 2*a++;
      else
         P+= 5 + 2*(a++ - b--);
    } while(a <= b);
}
