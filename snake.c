#include <stdint.h>
#include <stdio.h>
#include <RTL.h>
#include <MKL25Z4.h>

#include "TFT_LCD.h"
#include "font.h"
#include "tasks.h"
#include "MMA8451.h"
#include "sound.h"
#include "DMA.h"
#include "gpio_defs.h"
#include "snake.h"

extern os_mbx_declare (MsgBox,1);
extern U32 mpool[20*(2*sizeof(U32))/4+3];
extern OS_MUT LCD_mutex;
/*game status*/
bool game_on = false,status_mode=false, game_paused = false;

extern OS_TID t_Intro,t_Sound_Manager;
extern int track;

/* snake length */
static int snakeLen = SNAKE_LEN;
static int point = 0, points = 0;
int level = 0;
static int speed = SNAKE_SPEED;

/* coordinates of snake head */
static int xSnake,ySnake; 
/* coordinates of food */
static int xFood = 0, yFood  = 0; 

/*difference between current and next coordinates of head*/
static int dr = 0, dc = 1;
/* directions */
static bool left = false, right = true, up = false, down = false;

/* vector containing the coordinates of the individual parts */
static int snakeCols[260];

/* Vector containing the coordinates of the individual parts */
static int snakeRow[260];

//unsigned int dummycnt = 0;

int random(unsigned int min, unsigned int max) {
  unsigned int cnt = 10;
	static bool toggle = true;
	//get_stack_depth(CURRENT_STACK);
  cnt = os_time_get ();
	//get_stack_depth(MAX_STACK);
	
	while((cnt >= max) || (cnt <= min)){
		cnt = os_time_get ();	
		//cnt &= 0xfff; /* reduce to 12 bits */
		if(toggle){
			cnt = cnt >> (cnt& 0x0f);
		}
		else{
			cnt = cnt << (cnt & 0x0f);
		}
		cnt = cnt & 0xfff;
		if(cnt == 0){
			cnt = 1;
		}
		if (cnt<=min) {
			cnt = cnt<<1;
		}
		if (cnt>=max) {
			cnt = cnt>>1;
		}
	}
  return cnt;
}

void resetGame(void) {
  int i;  
	COLOR_T blue;
	blue.B = 200;
	blue.G = 0;
	blue.R = 0;
	
	//get_stack_depth(CURRENT_STACK);
  TFT_Erase();
	//TFT_Text_PrintStr_RC(1,0, "Ready?");
	os_mut_wait(&LCD_mutex, WAIT_FOREVER);
	TFT_Text_PrintStr_RC(2,0, "Loading Game");
	DrawFilledRectangle(0, 155, GAME_WIDTH/5,165, blue);
	TFT_Text_PrintStr_RC(8,9, "5");
	os_mut_release(&LCD_mutex);
	os_dly_wait(500);
	os_mut_wait(&LCD_mutex, WAIT_FOREVER);
	DrawFilledRectangle(GAME_WIDTH/5, 155, 2*GAME_WIDTH/5,165, blue);
	TFT_Text_PrintStr_RC(8,9, "4");
	os_mut_release(&LCD_mutex);
	os_dly_wait(500);
	os_mut_wait(&LCD_mutex, WAIT_FOREVER);
	DrawFilledRectangle(2*GAME_WIDTH/5, 155, 3*GAME_WIDTH/5,165, blue);
	TFT_Text_PrintStr_RC(8,9, "3");
	os_mut_release(&LCD_mutex);
	os_dly_wait(500);
	os_mut_wait(&LCD_mutex, WAIT_FOREVER);
	DrawFilledRectangle(3*GAME_WIDTH/5, 155, 4*GAME_WIDTH/5,165, blue);
	TFT_Text_PrintStr_RC(8,9, "2");
	os_mut_release(&LCD_mutex);
	os_dly_wait(500);
	os_mut_wait(&LCD_mutex, WAIT_FOREVER);
	DrawFilledRectangle(4*GAME_WIDTH/5, 155, 5*GAME_WIDTH/5,165, blue);
	TFT_Text_PrintStr_RC(8,9, "1");
	os_mut_release(&LCD_mutex);
	os_dly_wait(500);
	os_mut_wait(&LCD_mutex, WAIT_FOREVER);
	//DrawFilledRectangle(GAME_WIDTH/5, 155, 2*GAME_WIDTH/5,165, blue);
	TFT_Text_PrintStr_RC(8,9, "GO!");
	TFT_Erase();
	os_mut_release(&LCD_mutex);
	
	xSnake = (GAME_WIDTH/4);
  ySnake = (GAME_HEIGHT/2);
  snakeLen = SNAKE_LEN;
  for(i=0; i < (snakeLen-1); i++) {
    snakeCols[i] = xSnake-i;
    snakeRow[i]  = (GAME_HEIGHT / 2);
  }	
  xFood = (GAME_WIDTH/2);
  yFood = (GAME_HEIGHT/2); 
  level  = 0;  
  point  = -1;
  points = 0;
  speed   = SNAKE_SPEED;
  up    = false; 
  right = true;
  down  = false;
  left  = false;
  dr    = 0;
  dc    = 1;
//	black.B = 0;black.G = 0;black.R = 0;
//	DrawFilledRectangle(5, 5, GAME_WIDTH, GAME_HEIGHT, black);
}

void drawSnake(void) {
  int i;
	COLOR_T black,red,green;
	black.B = 0;black.G = 0;black.R = 0;
	red.R = 200;red.G = 0;red.B = 0;
	green.R = 0;green.G = 200;green.B = 0;	
	//get_stack_depth(CURRENT_STACK);
  for(i = 0; i < snakeLen; i+=3) {
		os_mut_wait(&LCD_mutex, WAIT_FOREVER);
    DrawFilledCircle(snakeCols[i], snakeRow[i], 2, red);
		os_mut_release(&LCD_mutex);
  }
	os_mut_wait(&LCD_mutex, WAIT_FOREVER);
  DrawFilledCircle(xFood, yFood, 2,green);
  DrawFilledCircle(snakeCols[snakeLen], snakeRow[snakeLen], 2, black); 
	os_mut_release(&LCD_mutex);
  for(i = snakeLen; i > 0; i--) {
    snakeRow[i]  = snakeRow[i - 1];
    snakeCols[i] = snakeCols[i - 1];
  }
  snakeRow[0]  += dr;
  snakeCols[0] += dc;
}


void eatFood(void) {
	int i;
	COLOR_T black;
	black.B = 0;black.G = 0;black.R = 0;
  point++;
	get_stack_depth(CURRENT_STACK);
	for(i = snakeLen+10; i > snakeLen; i--) {
    snakeRow[i]  = snakeRow[i - 10];
    snakeCols[i] = snakeCols[i - 10];
  }
  snakeLen += 10;
	os_mut_wait(&LCD_mutex, WAIT_FOREVER);
	DrawFilledCircle(xFood, yFood, 2,black);
	os_mut_release(&LCD_mutex);
  xFood = random(2, GAME_WIDTH-2);
  yFood = random(2, GAME_HEIGHT-2);
	//track = 0;
	os_evt_set(EV_PLAYSOUND_EAT_FOOD,t_Sound_Manager);
  drawSnake();  
}

void upLevel(void) {
  char buffer[100];
	game_paused = true;
	level++;
  point   = 0;
  points += 5;
	get_stack_depth(CURRENT_STACK);
	if(level > 5){
		os_mut_wait(&LCD_mutex, WAIT_FOREVER);
		TFT_Text_PrintStr_RC(1,0, "Congratulations");
		TFT_Text_PrintStr_RC(2,0, "You Won");
		TFT_Text_PrintStr_RC(3,0, "Tap To Reset");
		os_mut_release(&LCD_mutex);
		//os_dly_wait(1000);
		os_evt_wait_and(EV_RESUME_GAME, WAIT_FOREVER);
		os_evt_set(EV_RUN_INTRO,t_Intro);
		os_tsk_delete_self();
	}
	else{
		os_mut_wait(&LCD_mutex, WAIT_FOREVER);
		TFT_Text_PrintStr_RC(1,0, "Starting");
		sprintf(buffer,"LEVEL:%d",level);
		TFT_Text_PrintStr_RC(2,0,buffer);
		TFT_Text_PrintStr_RC(3,0, "To Advance");
		sprintf(buffer,"Score:%d Points",points);
		TFT_Text_PrintStr_RC(4,0, buffer);
		TFT_Text_PrintStr_RC(5,0, "Tap To Start");
		os_mut_release(&LCD_mutex);
		snakeLen = SNAKE_LEN;
		if(level > 1) {
			speed -= 4;
		}		
		//track = 2;
		os_evt_set(EV_PLAYSOUND_LVLUP,t_Sound_Manager);
	}
}

void direc(int d) {
  switch(d) {
    case UP:    { left=false; right=false; up=true ; down=false; dr = -1; dc =  0;} break;
    case RIGHT: { left=false; right=true ; up=false; down=false; dr =  0; dc =  1;} break;
    case DOWN:  { left=false; right=false; up=false; down=true ; dr =  1; dc =  0;} break;
    case LEFT:  { left=true ; right=false; up=false; down=false; dr =  0; dc = -1;} break;
  }
}

void moveSnake(void) {
	float *msgptr;
	float roll,pitch;
	//get_stack_depth(CURRENT_STACK);
	os_mbx_wait(MsgBox,(void*)&msgptr,WAIT_FOREVER);
	roll = msgptr[0];
	pitch = msgptr[1];
   _free_box (mpool, msgptr); 
	/* LEFT */
  if(roll < -15 && !right) {
      direc(LEFT);
    return;
  }
  /* RIGHT *///pos roll
  if(roll > 15 && !left) {
      direc(RIGHT);
    return;
  }
  /* UP */
  if(pitch < -15 && !down) {
      direc(UP);
    return;
  }
  /* DOWN *///pos pitch
  if(pitch > 15 && !up) {
      direc(DOWN);
    return;
  }
}

void gameover(void) {
  
	char buffer[15];
	//display game over
	get_stack_depth(CURRENT_STACK);
	os_mut_wait(&LCD_mutex, WAIT_FOREVER);
	TFT_Erase();
	TFT_Text_PrintStr_RC(1,0, "Game Over");
	sprintf(buffer,"LEVEL:%d",level);
	TFT_Text_PrintStr_RC(2, 0, buffer);
	sprintf(buffer,"Points:%d",point);
	TFT_Text_PrintStr_RC(3, 0, buffer);
	os_mut_release(&LCD_mutex);
	//track = 3;
	os_evt_set(EV_PLAYSOUND_GAMEOVER,t_Sound_Manager);
	os_mut_wait(&LCD_mutex, WAIT_FOREVER);
	TFT_Text_PrintStr_RC(4,0, "Tap To Reset");
	os_mut_release(&LCD_mutex);
	game_paused = true;
	os_evt_wait_and(EV_RESUME_GAME, WAIT_FOREVER);
	os_evt_set(EV_RUN_INTRO,t_Intro);
	os_tsk_delete_self();
	//kill task
}


void snake(void) {
  int i,j;

  if(point == -1 || point >= points) {
    upLevel();
  } 
	
	xSnake = snakeCols[0];
  ySnake = snakeRow[0];
	get_stack_depth(CURRENT_STACK);
	//todo
	//check if paused
	if(status_mode == true || game_paused == true){
		os_evt_wait_and(EV_RESUME_GAME, WAIT_FOREVER);	//wait for touch if paused
		TFT_Erase();
	}
	
	for(i=2;i<snakeLen;i++){
		for(j=0;j<2;j++){
			if(xSnake == snakeCols[i] && ySnake == snakeRow[i]){
				gameover();
				break;
			}
	/*		if(left || right){
				if((xSnake ==snakeCols[i] )&&(ySnake == snakeRow[i] || ySnake == snakeRow[i] +j||ySnake == snakeRow[i] -j)){
					gameover();
					break;
				}
			}
			if(up || down){
				if((ySnake ==snakeRow[i] )&&(xSnake == snakeCols[i] || xSnake == snakeCols[i] +j||xSnake == snakeCols[i] -j)){
					gameover();
					break;
				}
			}*/
		}
	}

	moveSnake();
	for(i=0;i<3;i++){
		if((xSnake ==xFood ||xSnake == xFood+i ||xSnake == xFood-i)&&(ySnake == yFood || ySnake == yFood +i||ySnake == yFood -i))
		{
			eatFood();
			break;
		}
	/*	if((xSnake +i ==xFood ||xSnake+i == xFood+i ||xSnake+i == xFood-i)&&(ySnake+i == yFood || ySnake+i == yFood +i||ySnake+i == yFood -i))
		{
			eatFood();
			break;
		}
		if((xSnake -i ==xFood ||xSnake-i == xFood+i ||xSnake-i == xFood-i)&&(ySnake-i == yFood || ySnake-i == yFood -i||ySnake-i == yFood -i))
		{
			eatFood();
			break;
		}
		if((xSnake +i ==xFood ||xSnake+i == xFood+i ||xSnake+i == xFood-i)&&(ySnake-i == yFood || ySnake-i == yFood +i||ySnake-i == yFood -i))
		{
			eatFood();
			break;
		}
			if((xSnake -i ==xFood ||xSnake-i == xFood+i ||xSnake-i == xFood-i)&&(ySnake+i == yFood || ySnake+i == yFood +i||ySnake+i == yFood -i))
		{
			eatFood();
			break;
		}
		*/
	}
	
  /* LEFT */
  if(left) {
    /* snake touches the left wall */
    if(xSnake == 3) {
      gameover();
    }
    if(xSnake  > 3) {
      drawSnake();
    }
  }
  /* RIGHT */
  if(right) {
    /* snake touches the top wall */
    if(xSnake == GAME_WIDTH-3) {
      gameover();
    }
    if(xSnake  < GAME_WIDTH-3) {
      drawSnake();
    }
  }
  /* UP */
  if(up) {
    /* snake touches the above wall */
    if(ySnake == 3) {
      gameover();
    }
    if(ySnake > 3) {
      drawSnake();
    }
  }
  /* DOWN */
  if(down) {
    /* snake touches the ground */
    if(ySnake == GAME_HEIGHT-3) {
      gameover();
    }
    if(ySnake  < GAME_HEIGHT-3) {
      drawSnake();
    }
  }
	os_dly_wait(speed);
}

void intro(void) {
	game_on = 0;
	
	get_stack_depth(CURRENT_STACK);
	
	os_mut_wait(&LCD_mutex, WAIT_FOREVER);
	TFT_Erase();
	TFT_Text_PrintStr_RC(1,0, "ECE561 project 4");
  TFT_Text_PrintStr_RC(2,0, "Snake Game");
	TFT_Text_PrintStr_RC(4,0, "Touch Here");
	TFT_Text_PrintStr_RC(5,0, "To Display Status");
	TFT_Text_PrintStr_RC(TFT_MAX_ROWS - 3 ,0, "Touch Here");
	TFT_Text_PrintStr_RC(TFT_MAX_ROWS - 2,0, "To Start Game");
	os_mut_release(&LCD_mutex);
}

