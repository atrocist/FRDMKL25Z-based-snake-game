#include <stdint.h>
#include <stdio.h>
#include <RTL.h>
#include <MKL25Z4.h>

#include <string.h>
#include "TFT_LCD.h"
#include "font.h"
#include "tasks.h"
#include "MMA8451.h"
#include "sound.h"
#include "DMA.h"
#include "gpio_defs.h"
#include "snake.h"
#include "timers.h"

//todo declare memory pool
U32 mpool[20*(2*sizeof(U32))/4+3];


//fixed memory 20 blocks of 8 bytes
//_declare_box(mpool,8,20);
U64 RA_Stack[32];
U64 RTS_Stack[32];
U64 Snake_Stack[64];
U64 Intro_Stack[32];
U64 Status_Stack[128];
U64 SM_Stack[64];
U64 SB_Stack[64];

extern bool game_on,status_mode,game_paused;

OS_TID t_Read_TS, t_Read_Accelerometer, t_Sound_Manager, t_US, t_Refill_Sound_Buffer,t_Snake,t_Status,t_Intro;
OS_MUT LCD_mutex;
OS_MUT TS_mutex;
//todo declare mailbox
os_mbx_declare (MsgBox,1);

unsigned long long cpu_ticks[10]; 
unsigned int current_stack_depth[10];
unsigned int max_stack_depth[10];

//unsigned int dummysp = 0;
//OS_TID dummytid;
bool os_is_ready = false;
void get_stack_depth(bool depth_type)
{
	OS_TID task_id;
	unsigned int SP;
	//
	task_id= os_tsk_self ();
	//dummytid= task_id;
	SP= __current_sp();
	//dummysp = SP;
	if(task_id == t_Read_TS){
		if(depth_type == true){
			current_stack_depth[0] = ((unsigned int)&RTS_Stack[31]) - SP + 8;
		}
		else{
			max_stack_depth[0] = ((unsigned int)&RTS_Stack[31]) - SP + 8;
		}
	}
	else if(task_id == t_Read_Accelerometer){
		if(depth_type == true){
			current_stack_depth[1] = ((unsigned int)&RA_Stack[31]) - SP + 8;
		}
		else{
			max_stack_depth[1] = ((unsigned int)&RA_Stack[31]) - SP + 8;
		}
	}
	else if(task_id == t_Sound_Manager){
		if(depth_type == true){
			current_stack_depth[2] = ((unsigned int)&SM_Stack[63]) - SP + 8;
		}
		else{
			max_stack_depth[2] = ((unsigned int)&SM_Stack[63]) - SP + 8;
		}
	}
	else if(task_id == t_Refill_Sound_Buffer){
		if(depth_type == true){
			current_stack_depth[3] = ((unsigned int)&SB_Stack[63]) - SP + 8;
		}
		else{
			max_stack_depth[3] = ((unsigned int)&SB_Stack[63]) - SP + 8;
		}
	}
	else if(task_id == t_Snake){
		if(depth_type == true){
			current_stack_depth[4] = ((unsigned int)&Snake_Stack[63]) - SP + 8;
		}
		else{
			max_stack_depth[4] = ((unsigned int)&Snake_Stack[63]) - SP + 8;
		}
	}
	else if(task_id == t_Status){
		if(depth_type == true){
			current_stack_depth[5] = ((unsigned int)&Status_Stack[127]) - SP + 8;
		}
		else{
			max_stack_depth[5] = ((unsigned int)&Status_Stack[127]) - SP + 8;
		}
	}
	else if(task_id == t_Intro){
		if(depth_type == true){
			current_stack_depth[6] = ((unsigned int)&Intro_Stack[31]) - SP + 8;
		}
		else{
			max_stack_depth[6] = ((unsigned int)&Intro_Stack[31]) - SP + 8;
		}
	}
	//
}

void Init_Debug_Signals(void) {
	// Enable clock to port B
	SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
	
	// Make pins GPIO
	PORTB->PCR[DEBUG_T0_POS] &= ~PORT_PCR_MUX_MASK;          
	PORTB->PCR[DEBUG_T0_POS] |= PORT_PCR_MUX(1);          
	PORTB->PCR[DEBUG_T1_POS] &= ~PORT_PCR_MUX_MASK;          
	PORTB->PCR[DEBUG_T1_POS] |= PORT_PCR_MUX(1);          
	PORTB->PCR[DEBUG_T2_POS] &= ~PORT_PCR_MUX_MASK;          
	PORTB->PCR[DEBUG_T2_POS] |= PORT_PCR_MUX(1);          
	PORTB->PCR[DEBUG_T3_POS] &= ~PORT_PCR_MUX_MASK;          
	PORTB->PCR[DEBUG_T3_POS] |= PORT_PCR_MUX(1);          

	PORTB->PCR[DEBUG_I0_POS] &= ~PORT_PCR_MUX_MASK;          
	PORTB->PCR[DEBUG_I0_POS] |= PORT_PCR_MUX(1);          

	
	// Set ports to outputs
	PTB->PDDR |= MASK(DEBUG_T0_POS);
	PTB->PDDR |= MASK(DEBUG_T1_POS);
	PTB->PDDR |= MASK(DEBUG_T2_POS);
	PTB->PDDR |= MASK(DEBUG_T3_POS);
	PTB->PDDR |= MASK(DEBUG_I0_POS);
	
	// Initial values are 0
	PTB->PCOR = MASK(DEBUG_T0_POS);
	PTB->PCOR = MASK(DEBUG_T1_POS);
	PTB->PCOR = MASK(DEBUG_T2_POS);
	PTB->PCOR = MASK(DEBUG_T3_POS);
	PTB->PCOR = MASK(DEBUG_I0_POS);

}	

__task void Task_Init(void) {
	os_mut_init(&LCD_mutex);
	
	_init_box (mpool, sizeof (mpool), 8);
	
	os_mbx_init (MsgBox, sizeof(MsgBox));
	memset(cpu_ticks,0,sizeof(cpu_ticks));
	
	cpu_ticks[7]=1;
	PIT_Init(2399); // 24 MHz/(2399+1) = 100 samples per second
	PIT_Start();
	
	t_Read_TS = os_tsk_create_user(Task_Read_TS, PRIO_READ_TS, RTS_Stack, 512/2);
	t_Intro = os_tsk_create_user(Task_Intro, PRIO_INTRO, Intro_Stack, 512/2);
	t_Status = os_tsk_create_user(Task_Status, PRIO_STATUS, Status_Stack, 2*512);
	t_Read_Accelerometer = os_tsk_create_user(Task_Read_Accelerometer, PRIO_READ_ACC, RA_Stack, 512/2);
	t_Sound_Manager = os_tsk_create_user(Task_Sound_Manager, PRIO_SM, SM_Stack, 512); // Should be lower priority than Refill Sound Buffer
	//t_US = os_tsk_create(Task_Update_Screen, 5);
	t_Refill_Sound_Buffer = os_tsk_create_user(Task_Refill_Sound_Buffer, PRIO_SB, SB_Stack, 512);
	os_is_ready = true;
  os_tsk_delete_self ();
}

__task void Task_Read_TS(void) {
	PT_T p;
	os_itv_set(TASK_READ_TS_PERIOD_TICKS);
	while (1) {
		//p.X = 0;
		//p.Y = 0;
		get_stack_depth(CURRENT_STACK);
		os_itv_wait();
		PTB->PSOR = MASK(DEBUG_T1_POS);
		if (TFT_TS_Read(&p)) { 
			// Send message indicating screen was pressed
		//	os_evt_set(EV_PLAYSOUND, t_PlaySound);//todo write sound task
			if(!game_on && !status_mode){
				//game is not running not paused
				if (p.Y > 250) { 
					//start a new game start button pressed
				//	status_mode = true;
					t_Snake = os_tsk_create_user(Task_Snake, PRIO_SNAKE,Snake_Stack,512);
					os_evt_set(EV_START_GAME,t_Snake);//todo write snake task
				}else if (p.Y < 150){
					//display status status button pressed
					os_evt_set(EV_DISPLAY_STATUS,t_Status);//todo write status task
				} 
			}else if(game_on && !status_mode && !game_paused){
				//game was on status display was requested (screen tapped)
				//display status
				status_mode = true;
				os_evt_set(EV_DISPLAY_STATUS,t_Status);//todo write status task
			}else if(game_on && status_mode && !game_paused){
				//game was paused screen tapped resume snake game
				status_mode = false;
				os_evt_set(EV_DISPLAY_STATUS,t_Status);
				os_evt_set(EV_PLAYSOUND_START,t_Sound_Manager);
				os_evt_set(EV_RESUME_GAME,t_Snake);//todo write snake task
			}else if(game_on && game_paused){
				game_paused = false;
				os_evt_set(EV_PLAYSOUND_START,t_Sound_Manager);
				os_evt_set(EV_RESUME_GAME,t_Snake);
			}
		}else {
			PTB->PCOR = MASK(DEBUG_T1_POS);
		}
	}
}
__task void Task_Read_Accelerometer(void) {
	//char buffer[16];
	float *msgptr;
	
	//os_itv_set(TASK_READ_ACCELEROMETER_PERIOD_TICKS);

	while (1) {
	//	os_itv_wait();
		PTB->PSOR = MASK(DEBUG_T0_POS);
		tsk_lock();
		read_full_xyz();
		tsk_unlock();
		convert_xyz_to_roll_pitch();
		msgptr = _alloc_box(mpool);
		msgptr[0] = roll;
		msgptr[1] = pitch;
		get_stack_depth(CURRENT_STACK);
		os_mbx_send (MsgBox, msgptr, WAIT_FOREVER);	
		
/*
		sprintf(buffer, "Roll: %6.2f", roll);
		os_mut_wait(&LCD_mutex, WAIT_FOREVER);
		TFT_Text_PrintStr_RC(2, 0, buffer);
		os_mut_release(&LCD_mutex);

		sprintf(buffer, "Pitch: %6.2f", pitch);
		os_mut_wait(&LCD_mutex, WAIT_FOREVER);
		TFT_Text_PrintStr_RC(3, 0, buffer);
		os_mut_release(&LCD_mutex);
*/
		PTB->PCOR = MASK(DEBUG_T0_POS);
	}
}

/*
__task void Task_Update_Screen(void) {
	int16_t paddle_pos=TFT_WIDTH/2;
	PT_T p1, p2;
	COLOR_T paddle_color, black;
	
	paddle_color.R = 100;
	paddle_color.G = 200;
	paddle_color.B = 50;

	black.R = 0;
	black.G = 0;
	black.B = 0;
	
	os_itv_set(TASK_UPDATE_SCREEN_PERIOD_TICKS);

	while (1) {
		os_itv_wait();
		PTB->PSOR = MASK(DEBUG_T3_POS);
		
		if ((roll < -2.0) || (roll > 2.0)) {
			p1.X = paddle_pos;
			p1.Y = PADDLE_Y_POS;
			p2.X = p1.X + PADDLE_WIDTH;
			p2.Y = p1.Y + PADDLE_HEIGHT;
			TFT_Fill_Rectangle(&p1, &p2, &black); 		
			
			paddle_pos += roll;
			paddle_pos = MAX(0, paddle_pos);
			paddle_pos = MIN(paddle_pos, TFT_WIDTH-1);
			
			p1.X = paddle_pos;
			p1.Y = PADDLE_Y_POS;
			p2.X = p1.X + PADDLE_WIDTH;
			p2.Y = p1.Y + PADDLE_HEIGHT;
			TFT_Fill_Rectangle(&p1, &p2, &paddle_color); 		
		}
		
		PTB->PCOR = MASK(DEBUG_T3_POS);
	}
}
*/ //dont need this now 


__task void Task_Intro(void){
	while(1){
		
		get_stack_depth(CURRENT_STACK);
		
		intro();
		//todo create snake task
		game_on = false;
		status_mode = false;		
		os_evt_wait_and(EV_RUN_INTRO, WAIT_FOREVER);
	
	}
}

void display_status(){
	char buffer[50];
	unsigned long long total_ticks = 0;
	int i;
	
	get_stack_depth(CURRENT_STACK);
	
	//TFT_Text_PrintStr_RC(1,0, "ECE561 project 4");
  TFT_Text_PrintStr_RC(0,0, "Status Display");
	TFT_Text_PrintStr_RC(1,0, "Tap To Go Back");
	//TFT_Text_PrintStr_RC(4,0, "To Go Back");
	for(i=0;i<8;i++)
		total_ticks += cpu_ticks[i];
	
	os_mut_wait(&LCD_mutex, WAIT_FOREVER);
	//
	TFT_Text_PrintStr_RC(2,0, "Task Name:%CPU");
	sprintf(buffer, "TS:%.4f Ac:%.4f", 100.0*(float)cpu_ticks[0]/(float)total_ticks,100.0*(float)cpu_ticks[1]/(float)total_ticks);
	TFT_Text_PrintStr_RC(3,0, buffer);
	sprintf(buffer, "SM:%.4f SB:%.4f", 100.0*(float)cpu_ticks[2]/(float)total_ticks,100.0*(float)cpu_ticks[4]/(float)total_ticks);
	TFT_Text_PrintStr_RC(4,0, buffer);
	sprintf(buffer, "Snk:%.4f Sts:%.4f", 100.0*(float)cpu_ticks[4]/(float)total_ticks, 100.0*(float)cpu_ticks[5]/(float)total_ticks);
	TFT_Text_PrintStr_RC(5,0, buffer);
	sprintf(buffer, "Int:%.4f Idl:%.4f", 100.0*(float)cpu_ticks[6]/(float)total_ticks, 100.0*(float)cpu_ticks[7]/(float)total_ticks);
	TFT_Text_PrintStr_RC(6,0, buffer);
	sprintf(buffer, "Tot:%.4f", 100.0*(float)(total_ticks-cpu_ticks[7])/(float)total_ticks);
	TFT_Text_PrintStr_RC(7,0, buffer);
	
	TFT_Text_PrintStr_RC(8,0, "Task Name:Cur/Max");
	sprintf(buffer,"TS:%.4f/%.4f",100.0f*(float)(current_stack_depth[0])/256.0f,100.0f*(float)(max_stack_depth[0])/256.0f);
	TFT_Text_PrintStr_RC(9,0, buffer);
	sprintf(buffer,"AC:%.4f/%.4f",100.0f*(float)(current_stack_depth[1])/256.0f,100.0f*(float)(max_stack_depth[1])/256.0f);
	TFT_Text_PrintStr_RC(10,0, buffer);
	sprintf(buffer,"SM:%.4f/%.4f",100.0f*(float)(current_stack_depth[2])/512.0f,100.0f*(float)(max_stack_depth[2])/512.0f);
	TFT_Text_PrintStr_RC(11,0, buffer);
	sprintf(buffer,"SB:%.4f/%.4f",100.0f*(float)(current_stack_depth[3])/512.0f,100.0f*(float)(max_stack_depth[3])/512.0f);
	TFT_Text_PrintStr_RC(12,0, buffer);
	sprintf(buffer,"SNK:%.4f/%.4f",100.0f*(float)(current_stack_depth[4])/512.0f,100.0f*(float)(max_stack_depth[4])/512.0f);
	TFT_Text_PrintStr_RC(13,0, buffer);
	sprintf(buffer,"STS:%.4f/%.4f",100.0f*(float)(current_stack_depth[5])/1024.0f,100.0f*(float)(max_stack_depth[5])/1024.0f);
	TFT_Text_PrintStr_RC(14,0, buffer);
	sprintf(buffer,"INT:%.4f/%.4f",100.0f*(float)(current_stack_depth[6])/256.0f,100.0f*(float)(max_stack_depth[6])/256.0f);
	TFT_Text_PrintStr_RC(15,0, buffer);
	os_mut_release(&LCD_mutex);
	//
}

__task void Task_Status(void){
	while(1){
		get_stack_depth(CURRENT_STACK);
		os_evt_wait_and(EV_DISPLAY_STATUS, WAIT_FOREVER);
		os_mut_wait(&LCD_mutex, WAIT_FOREVER);
		os_mut_release(&LCD_mutex);
		TFT_Erase();
		while(os_evt_wait_and(EV_DISPLAY_STATUS, 100) != OS_R_EVT){
			display_status();
		}
		if(!game_on){		
			os_evt_set(EV_RUN_INTRO,t_Intro);
		}//else if(status_mode){
			//status_mode = false;
		//}
	}
}

__task void Task_Snake(void){
	//os_itv_set(TASK_SNAKE_PERIOD_TICKS);
	os_evt_wait_and(EV_START_GAME, WAIT_FOREVER);
	game_on = 1;
	resetGame();
	
	while(1){
		get_stack_depth(CURRENT_STACK);
	//	os_dly_wait(1);
		snake();
	}
}


