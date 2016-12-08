#include "timers.h"
#include "MKL25Z4.h"
#include "region.h"
#include "profile.h"
#include "tasks.h"
#include <RTL.h>
volatile unsigned PIT_interrupt_counter = 0;
volatile unsigned LCD_update_requested = 0;
unsigned int PC_val = 0;

extern unsigned profile_ticks;
extern volatile char profiling_enabled;
extern OS_TID t_Read_TS, t_Read_Accelerometer, t_Sound_Manager, t_US, t_Refill_Sound_Buffer,t_Snake,t_Status,t_Intro;
extern unsigned long long cpu_ticks[10];
extern bool os_is_ready;
extern volatile unsigned int adx_lost, num_lost; 

void PIT_IRQHandler() {
	OS_TID task_id;
	unsigned int s, e;
  unsigned int i;
	
	//clear pending IRQ
	NVIC_ClearPendingIRQ(PIT_IRQn);
	
	// check to see which channel triggered interrupt 
	if (PIT->CHANNEL[0].TFLG & PIT_TFLG_TIF_MASK) {
		// clear status flag for timer channel 0
		PIT->CHANNEL[0].TFLG &= PIT_TFLG_TIF_MASK;
		
		// Do ISR work
		// Profiler
		if (profiling_enabled > 0) {
			PC_val = *((unsigned int *) (__current_sp()+CUR_FRAME_SIZE+RET_ADX_OFFSET));
			profile_ticks++;
  	
			/* look up function in table and increment counter  */
			for (i=0; i<NumProfileRegions; i++) {
				s = RegionTable[i].Start;
				e = RegionTable[i].End;
				if ((PC_val >= s) && (PC_val <= e)) {
					RegionCount[i]++;
					break; // break out of the for loop
				}
			}
			if (i == NumProfileRegions) {
				adx_lost = PC_val;
				num_lost++;
			}
		}else{
			task_id = isr_tsk_get();
		
		if(task_id == t_Read_TS)
			(cpu_ticks[0])++;
		else if(task_id == t_Read_Accelerometer)
			(cpu_ticks[1])++;	
		else if(task_id == t_Sound_Manager)
			(cpu_ticks[2])++;
		else if(task_id == t_Refill_Sound_Buffer)
			(cpu_ticks[3])++;
		else if(task_id == t_Snake)
			(cpu_ticks[4])++;
		else if(task_id == t_Status)
			(cpu_ticks[5])++;
		else if(task_id == t_Intro)
			(cpu_ticks[6])++;
		else
			(cpu_ticks[7])++;
		
		}
	} else if (PIT->CHANNEL[1].TFLG & PIT_TFLG_TIF_MASK) {
		// clear status flag for timer channel 1
		PIT->CHANNEL[1].TFLG &= PIT_TFLG_TIF_MASK;
		
	} 
}

void PIT_Init(unsigned period) {
	// Enable clock to PIT module
	SIM->SCGC6 |= SIM_SCGC6_PIT_MASK;
	
	// Enable module, freeze timers in debug mode
	PIT->MCR &= ~PIT_MCR_MDIS_MASK;
	PIT->MCR |= PIT_MCR_FRZ_MASK;
	
	// Initialize PIT0 to count down from argument 
	PIT->CHANNEL[0].LDVAL = PIT_LDVAL_TSV(period);

	// No chaining
	PIT->CHANNEL[0].TCTRL &= PIT_TCTRL_CHN_MASK;
	
	// Generate interrupts
	PIT->CHANNEL[0].TCTRL |= PIT_TCTRL_TIE_MASK;

	/* Enable Interrupts */
	NVIC_SetPriority(PIT_IRQn, 128); // 0, 64, 128 or 192
	NVIC_ClearPendingIRQ(PIT_IRQn); 
	NVIC_EnableIRQ(PIT_IRQn);	
}


void PIT_Start(void) {
// Enable counter
	PIT->CHANNEL[0].TCTRL |= PIT_TCTRL_TEN_MASK;
}

void PIT_Stop(void) {
// Enable counter
	PIT->CHANNEL[0].TCTRL &= ~PIT_TCTRL_TEN_MASK;
}

void Init_PWM(TPM_Type * TPM, uint8_t channel_num, uint16_t period, uint16_t duty)
{
		//turn on clock to TPM 
		switch ((int) TPM) {
			case (int) TPM0:
				SIM->SCGC6 |= SIM_SCGC6_TPM0_MASK;
				break;
			case (int) TPM1:
				SIM->SCGC6 |= SIM_SCGC6_TPM1_MASK;
				break;
			case (int) TPM2:
				SIM->SCGC6 |= SIM_SCGC6_TPM2_MASK;
				break;
			default:
				break;
		}
		//set clock source for tpm
		SIM->SOPT2 |= (SIM_SOPT2_TPMSRC(1) | SIM_SOPT2_PLLFLLSEL_MASK);
		
		if (channel_num == 0) {
			//set multiplexer to connect TPM1 Ch 0 to PTA12
			PORTA->PCR[12] &= PORT_PCR_MUX_MASK; 
			PORTA->PCR[12] |= PORT_PCR_MUX(3); 
		}

		//load the counter and mod
		TPM->MOD = period;
			
		//set channels to center-aligned high-true PWM
		TPM->CONTROLS[channel_num].CnSC = TPM_CnSC_MSB_MASK | TPM_CnSC_ELSB_MASK;

		//set TPM to up-down and divide by 8 prescaler and clock mode
		TPM->SC = (TPM_SC_CPWMS_MASK | TPM_SC_CMOD(1) | TPM_SC_PS(3));
		
		//set trigger mode
		TPM->CONF |= TPM_CONF_TRGSEL(0xA);

		// Set initial duty cycle
		TPM->CONTROLS[channel_num].CnV = duty;
}

void Old_Init_PWM(void)
{
	//turn on clock to TPM 
	SIM->SCGC6 |= SIM_SCGC6_TPM1_MASK;

	//set multiplexer to connect TPM1 Ch 0 to PTA12
	PORTA->PCR[12] |= PORT_PCR_MUX(3); 

	//set clock source for tpm
	SIM->SOPT2 |= (SIM_SOPT2_TPMSRC(1) | SIM_SOPT2_PLLFLLSEL_MASK);

	//load the counter and mod
	TPM1->MOD = 10000;
		
	//set channels to center-aligned high-true PWM
	TPM1->CONTROLS[0].CnSC = TPM_CnSC_MSB_MASK | TPM_CnSC_ELSB_MASK;

	//set TPM to up-down and divide by 8 prescaler and clock mode
	TPM1->SC = (TPM_SC_CPWMS_MASK | TPM_SC_CMOD(1) | TPM_SC_PS(3));
	
	//set trigger mode
	TPM1->CONF |= TPM_CONF_TRGSEL(0xA);
	
	// Set duty cycle
	TPM1->CONTROLS[0].CnV = 9000;
}

void PWM_Set_Value(TPM_Type * TPM, uint8_t channel_num, uint16_t value) {
	TPM->CONTROLS[channel_num].CnV = value;
}


void TPM0_Init(void) {
	//turn on clock to TPM 
	SIM->SCGC6 |= SIM_SCGC6_TPM0_MASK;
	
	//set clock source for tpm
	SIM->SOPT2 |= (SIM_SOPT2_TPMSRC(1) | SIM_SOPT2_PLLFLLSEL_MASK);
}

void Configure_TPM0_for_DMA(uint32_t period_us)
{

	// disable TPM
	TPM0->SC = 0;
	
	//load the counter and mod
	TPM0->MOD = TPM_MOD_MOD(period_us*48);

	//set TPM to count up and divide by 1 prescaler and clock mode
	TPM0->SC = (TPM_SC_DMA_MASK | TPM_SC_PS(0));
	
#if 0 // if using interrupt for debugging
	// Enable TPM interrupts for debugging
	TPM0->SC |= TPM_SC_TOIE_MASK;

	// Configure NVIC 
	NVIC_SetPriority(TPM0_IRQn, 128); // 0, 64, 128 or 192
	NVIC_ClearPendingIRQ(TPM0_IRQn); 
	NVIC_EnableIRQ(TPM0_IRQn);	
#endif

}

void TPM0_Start(void) {
// Enable counter
	//if(os_is_ready){
	//	get_stack_depth(CURRENT_STACK);
	//}
	TPM0->SC |= TPM_SC_CMOD(1);
}



void TPM0_IRQHandler() {
	//clear pending IRQ
	NVIC_ClearPendingIRQ(TPM0_IRQn);
	
	TPM0->SC |= TPM_SC_TOF_MASK; 
}
// *******************************ARM University Program Copyright © ARM Ltd 2013*************************************   
