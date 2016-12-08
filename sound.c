#include <stdint.h>
#include <stdbool.h>
#include <MKL25Z4.h>
#include <math.h>
#include <RTL.h>

#include "sound.h"
#include "delay.h"
#include "gpio_defs.h"
#include "timers.h"
#include "DMA.h"
#include "tasks.h"

int16_t SineTable[NUM_STEPS];
uint16_t Waveform[NUM_WAVEFORM_SAMPLES];
//int track = 0;
extern bool game_on,status_mode,game_paused;
extern int level;
VOICE_T Voice[NUM_VOICES];

void DAC_Init(void) {
  // Init DAC output
	
	SIM->SCGC6 |= MASK(SIM_SCGC6_DAC0_SHIFT); 
	SIM->SCGC5 |= MASK(SIM_SCGC5_PORTE_SHIFT); 
	
	PORTE->PCR[DAC_POS] &= ~PORT_PCR_MUX_MASK;	
	PORTE->PCR[DAC_POS] |= PORT_PCR_MUX(0);	// Select analog 
		
	// Disable buffer mode
	DAC0->C1 = 0;
	DAC0->C2 = 0;
	
	// Enable DAC, select VDDA as reference voltage
	DAC0->C0 = MASK(DAC_C0_DACEN_SHIFT) | MASK(DAC_C0_DACRFS_SHIFT);
}

/*
	Code for driving DAC
*/
void Play_Sound_Sample(uint16_t val) {
	DAC0->DAT[0].DATH = DAC_DATH_DATA1(val >> 8);
	DAC0->DAT[0].DATL = DAC_DATL_DATA0(val);
}

void SineTable_Init(void) {
	unsigned n;
	
	for (n=0; n<NUM_STEPS; n++) {
		SineTable[n] = (MAX_DAC_CODE/2)*sinf(n*(2*3.1415927/NUM_STEPS));
	}
}

/* Fill waveform buffer with silence. */
void Init_Waveform(void) {
	uint32_t i;
	
	for (i=0; i<NUM_WAVEFORM_SAMPLES; i++) {
		Waveform[i] = (MAX_DAC_CODE/2);
	}
}

void Init_Voices(void) {
	uint16_t i;
	
	for (i=0; i<NUM_VOICES; i++) {
		Voice[i].Volume = 0;
		Voice[i].Decay = 0;
		Voice[i].Duration = 0;
		Voice[i].Period = 0;
		Voice[i].Counter = 0;
		Voice[i].CounterIncrement = 0;
		Voice[i].Type = VW_UNINIT;
	}
}

/* Initialize sound hardware, sine table, and waveform buffer. */
void Sound_Init(void) {
	SineTable_Init();	
	Init_Waveform();
	Init_Voices();
	
	DAC_Init();
	DMA_Init();
	TPM0_Init();
	Configure_TPM0_for_DMA(AUDIO_SAMPLE_PERIOD_US); 

	SIM->SOPT2 |= (SIM_SOPT2_TPMSRC(1) | SIM_SOPT2_PLLFLLSEL_MASK);


	SIM->SCGC5 |= (1UL << SIM_SCGC5_PORTE_SHIFT); 
	
	PORTE->PCR[AMP_ENABLE_POS] &= ~PORT_PCR_MUX_MASK;	
	PORTE->PCR[AMP_ENABLE_POS] |= PORT_PCR_MUX(1);	// Select GPIO
	PTE->PDDR |= MASK(AMP_ENABLE_POS); // set to output
	PTE->PSOR = MASK(AMP_ENABLE_POS);  // enable audio amp

}

void Sound_Enable_Amp(void) {
	PTE->PSOR = MASK(AMP_ENABLE_POS);  // enable audio amp
}

void Sound_Disable_Amp(void) {
	PTE->PCOR = MASK(AMP_ENABLE_POS);  // disable audio amp
}

/* Simple audio test function using busy-waiting. */
void Play_Tone(void) {
	int n, d=MAX_DAC_CODE>>1, p;
	
	for (p=5; p>=1; p--) {
		for (n=0; n<20/p; n++) {
			Play_Sound_Sample((MAX_DAC_CODE>>1)+d);
			Delay(p);
			Play_Sound_Sample((MAX_DAC_CODE>>1)-d);
			Delay(p);
		}
	}
}

int16_t Sound_Generate_Next_Sample (VOICE_T *voice) {
	uint16_t lfsr;
	uint16_t bit;
	int16_t sample;
	
	get_stack_depth(MAX_STACK);
	
	switch (voice->Type) {
		case VW_NOISE:
			lfsr = voice->Counter;
			// source code from http://en.wikipedia.org/wiki/Linear_feedback_shift_register
			/* taps: 16 14 13 11; characteristic polynomial: x^16 + x^14 + x^13 + x^11 + 1 */
			bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
			lfsr =  (lfsr >> 1) | (bit << 15);
			voice->Counter = lfsr;
			sample = (lfsr >> 4) - (MAX_DAC_CODE/2); // scale to get 12-bit value
			break;
		case VW_SQUARE:
			if (voice->Counter < voice->Period/2) {
				sample = MAX_DAC_CODE/2 - 1;
			} else {
				sample = -MAX_DAC_CODE/2;
			}
			voice->Counter++;
			if (voice->Counter == voice->Period) {
				voice->Counter = 0;
			}
			break;
		case VW_SINE:
			sample = SineTable[((voice->Counter)/256)]; // & (NUM_STEPS-1)]; 
			voice->Counter += voice->CounterIncrement;
			if (voice->Counter > voice->Period * voice->CounterIncrement){
				voice->Counter = 0;
			}
			break;
		default:
			sample = 0;
			break;
	}
	return sample;
}

void Play_Waveform_with_DMA(void) {
	
		Configure_DMA_For_Playback(Waveform, NUM_WAVEFORM_SAMPLES, 1);
		Start_DMA_Playback();
	
}

__task void Task_Sound_Manager(void) {
	uint32_t n=0,evt = OS_R_TMO;
//	uint16_t lfsr=1234;
//	uint16_t bit;
//	os_itv_set(100);
	
	while (1) {
		//os_itv_wait();
		get_stack_depth(CURRENT_STACK);	
		if(game_on && !status_mode && !game_paused)
				evt = os_evt_wait_or(EV_PLAYSOUND_EAT_FOOD|EV_PLAYSOUND_GAMEOVER|EV_PLAYSOUND_LVLUP, 300 - (level*50)); // wait for trigger
		else
				evt = os_evt_wait_or(EV_PLAYSOUND_EAT_FOOD|EV_PLAYSOUND_GAMEOVER|EV_PLAYSOUND_LVLUP|EV_PLAYSOUND_START, WAIT_FOREVER); // wait for trigger
		// make a new sound every second
		
		
		
		// Example code
		/*if (n < 200) {*/
		if(evt==OS_R_TMO || (EV_PLAYSOUND_START == os_evt_get())){/*
				Voice[n&0x03].Volume = 0x1FFF; 
				Voice[n&0x03].Duration = 3000;
			//	bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
			//	lfsr =  (lfsr >> 1) | (bit << 15);
				Voice[n&0x03].Period = FREQ_TO_PERIOD((lfsr & 0x03FF) + 100); 
				Voice[n&0x03].Decay = 25;
				Voice[n&0x03].Counter = 0; 
				Voice[n&0x03].CounterIncrement = (NUM_STEPS*256)/Voice[n&0x03].Period; 
				Voice[n&0x03].Type = VW_SINE;
				n++;*/
				switch (n & 3) {
				case 0:
					Voice[0].Volume = 0x1FFF;
					Voice[0].Duration = 11000;
					Voice[0].Decay = 40;
					Voice[0].Counter = 0xACE1u; // or 0 for non-noise
					Voice[0].Type = VW_NOISE;
					break;
				case 1:
					Voice[1].Volume = 0x1FFF; 
					Voice[1].Duration = 11000;
					Voice[1].Period = FREQ_TO_PERIOD(400);
					Voice[1].Decay = 20;
					Voice[1].Counter = 0; 
					Voice[1].Type = VW_SQUARE;
					break;
				default:
					Voice[2].Volume = 0x1FFF; 
					Voice[2].Duration = 11000;
					Voice[2].Period = FREQ_TO_PERIOD(700); 
					Voice[2].Decay = 10;
					Voice[2].Counter = 0; 
					Voice[2].CounterIncrement = (NUM_STEPS*256)/Voice[2].Period; 
					Voice[2].Type = VW_SINE;
					break;
			}
			//	if(n>3)
			//		n=0;
			n++;
		}
			/*} else if (n == 200) {
			os_itv_set(250);
		} else {*/
		else{
			switch (os_evt_get()) {
				case EV_PLAYSOUND_EAT_FOOD:
					Voice[0].Volume = 0xFFFF;
					Voice[0].Duration = 22000;
					Voice[0].Decay = 40;
					Voice[0].Counter = 0xACE1u; // or 0 for non-noise
					Voice[0].Type = VW_NOISE;
					break;
				
				case EV_PLAYSOUND_LVLUP:			
					Voice[2].Volume = 0xFFFF; 
					Voice[2].Duration = 22000;
					Voice[2].Period = FREQ_TO_PERIOD(700); 
					Voice[2].Decay = 10;
					Voice[2].Counter = 0; 
				//	Voice[2].CounterIncrement = (NUM_STEPS*256)/Voice[2].Period; 
					Voice[2].Type = VW_SINE;
					break;
				
				case EV_PLAYSOUND_GAMEOVER:
					Voice[3].Volume = 0xFFFF; 
					Voice[3].Duration = 22000;
					Voice[3].Period = FREQ_TO_PERIOD(100); 
					Voice[3].Decay = 10;
					Voice[3].Counter = 0; 
					Voice[3].CounterIncrement = (NUM_STEPS*256)/Voice[3].Period; 
					Voice[3].Type = VW_SQUARE;
					break;
				
				default:
					Voice[1].Volume = 0xFFFF; 
					Voice[1].Duration = 11000;
					Voice[1].Period = FREQ_TO_PERIOD(400);
					Voice[1].Decay = 20;
					Voice[1].Counter = 0; 
					Voice[1].Type = VW_SQUARE;
					break;
			}
		}
		//}
		//n++;
		
			os_evt_set(EV_REFILL_SOUND, t_Refill_Sound_Buffer);	
			Play_Waveform_with_DMA();
	}
}

__task void Task_Refill_Sound_Buffer(void) {
	uint32_t i;
	uint16_t v;
	
	int32_t sum, sample;

	while (1) {
		get_stack_depth(CURRENT_STACK);
		os_evt_wait_and(EV_REFILL_SOUND, WAIT_FOREVER); // wait for trigger
		PTB->PSOR = MASK(DEBUG_T2_POS);

		for (i=0; i<NUM_WAVEFORM_SAMPLES; i++) {
			sum = 0;
			for (v=0; v<NUM_VOICES; v++) {
				if (Voice[v].Duration > 0) {
					sample = Sound_Generate_Next_Sample(&(Voice[v]));
					
					sample = (sample*Voice[v].Volume)>>16;
					sum += sample;
					// update volume with decayed version
					Voice[v].Volume = (Voice[v].Volume * (((int32_t) 65536) - Voice[v].Decay)) >> 16; 
					Voice[v].Duration--;
				} 
			}
			sum = sum + (MAX_DAC_CODE/2);
			sum = MIN(sum, MAX_DAC_CODE-1);
			Waveform[i] = sum; ; 
		}
		PTB->PCOR = MASK(DEBUG_T2_POS);
	}
}

// Future expansion code below disabled
#if 0
const float startup_sound[] = {740, 880, 622.3, 740};
const float startup_chord[] = {659.3, 493.9, 329.6, 246.9};

__task void Task_Sequencer(void) {
	uint32_t p = 128;
	uint32_t n=0, v=0;
	os_itv_set(1000);
	
	while (1) {
		os_itv_wait();
		if (n<4) {
			v = 0;
			Voice[v].Volume = 0xFFFF; 
			Voice[v].Duration = 10000;
			Voice[v].Period = FREQ_TO_PERIOD(startup_sound[n]); 
			Voice[v].Decay = 20;
			Voice[v].Counter = 0; 
			Voice[v].CounterIncrement = (NUM_STEPS*256)/Voice[v].Period; 
			Voice[v].Type = VW_SINE;

			Voice[1].Duration = 0;
			Voice[2].Duration = 0;
			Voice[3].Duration = 0;

			n++;
		} else if (n==4) {
			for (v=0; v<4; v++) {
				Voice[v].Volume = 0xE000; 
				Voice[v].Duration = 60000;
				Voice[v].Period = FREQ_TO_PERIOD(startup_chord[v]); 
				Voice[v].Decay = 5;
				Voice[v].Counter = 0; 
				Voice[v].CounterIncrement = (NUM_STEPS*256)/Voice[v].Period; 
				Voice[v].Type = VW_SINE;
			}
			n++;
		}
		os_evt_set(EV_REFILL_SOUND, t_Refill_Sound_Buffer);	
		Play_Waveform_with_DMA();
	}
}
#endif
