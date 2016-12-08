#include <MKL25Z4.H>
#include "tasks.h"
#include "stdbool.h"

extern bool os_is_ready;
void Delay (uint32_t dly) {
  volatile uint32_t t;
	if(os_is_ready){
		get_stack_depth(MAX_STACK);
	}	
	for (t=dly*10000; t>0; t--)
		;
}

void ShortDelay (uint32_t dly) {
  volatile uint32_t t;

	for (t=dly; t>0; t--)
		;
}

// *******************************ARM University Program Copyright © ARM Ltd 2013*************************************   
