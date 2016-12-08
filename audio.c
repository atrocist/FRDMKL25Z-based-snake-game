#include <stdint.h>
#include <MKL25Z4.h>


unsigned SineTable[NUM_STEPS];

void Init_DAC(void) {
  // Init DAC output
	
	SIM->SCGC6 |= (1UL << SIM_SCGC6_DAC0_SHIFT); 
	SIM->SCGC5 |= (1UL << SIM_SCGC5_PORTE_SHIFT); 
	
	PORTE->PCR[DAC_POS] &= ~(PORT_PCR_MUX(7));	// Select analog 
		
	// Disable buffer mode
	DAC0->C1 = 0;
	DAC0->C2 = 0;
	
	// Enable DAC, select VDDA as reference voltage
	DAC0->C0 = (1 << DAC_C0_DACEN_SHIFT) | 
							(1 << DAC_C0_DACRFS_SHIFT);

}

void Delay_us(volatile unsigned int time_del) {
	// This is a very imprecise and fragile implementation!
	time_del = 9*time_del + time_del/2; 
	while (time_del--) {
		;
	}
}

/*------------------------------------------------------------------------------
	Code for driving DAC
	period: microseconds
	duration: cycles
*------------------------------------------------------------------------------*/
void Play_Tone(unsigned int period, unsigned int num_cycles, unsigned wave_type) {
	unsigned step, out_data;
	
	while (num_cycles>0) {
		num_cycles--;
		for (step = 0; step < NUM_STEPS; step++) {
			switch (wave_type) {
				case SQUARE: 
					if (step < NUM_STEPS/2)
						out_data = 0;	
					else
						out_data = MAX_DAC_CODE;
					break;
				case RAMP:
					out_data = (step*MAX_DAC_CODE)/NUM_STEPS;
					break;
				case SINE:
					out_data = SineTable[step];
					break;
			default:
					break;
		}
		DAC0->DAT[0].DATH = DAC_DATH_DATA1(out_data >> 8);
		DAC0->DAT[0].DATL = DAC_DATL_DATA0(out_data);
		Delay_us(period/NUM_STEPS);
		}
	}
}

void Init_SineTable(void) {
	unsigned n;
	
	for (n=0; n<NUM_STEPS; n++) {
		SineTable[n] = (MAX_DAC_CODE/2)*(1+sin(n*2*3.1415927/NUM_STEPS));
	}
}
