#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "timers.h"
#include "region.h"
#include "profile.h"
 
#include "TFT_LCD.h"
#include "font.h"

volatile unsigned int adx_lost=0, num_lost=0; 
volatile unsigned long profile_ticks=0;
volatile char profiling_enabled = 0;

void Init_Profiling(void) {
	unsigned i;
	
	// Clear region counts
  for (i=0; i<NumProfileRegions; i++) {
	  RegionCount[i]=0;
  }
	
	// Initialize and start timer
	PIT_Init(2399); // 24 MHz/(23999+1) = 1000 samples per second
	PIT_Start();
}

void Disable_Profiling(void) {
  profiling_enabled--;
}

void Enable_Profiling(void) {
  profiling_enabled++;
}

void Sort_Profile_Regions(void){
	unsigned i, j, t, iMax;
	
	// Initialize indices
  for (i=0; i<NumProfileRegions; i++) {
	  SortedRegions[i]=i;
  }
	
	// Sort indices with selection sort from Wikipedia
 
	/* advance the position through the entire array */
	/*   (could do j < n-1 because single element is also max element) */
	for (j = 0; j < NumProfileRegions-1; j++) {
    /* find the min element in the unsorted a[j .. n-1] */
 
    /* assume the max is the first element */
    iMax = SortedRegions[j];
    /* test against elements after j to find the smallest */
    for ( i = j+1; i < NumProfileRegions; i++) {
        /* if this element is more, then it is the new maximum */  
        if (RegionCount[SortedRegions[i]] > RegionCount[SortedRegions[iMax]]) {
            /* found new max; remember its index */
            iMax = i;
        }
    }
 
    /* iMax is the index of the maximum element. Swap it with the current position */
    if ( iMax != j ) {
				t = SortedRegions[j];
				SortedRegions[j] = SortedRegions[iMax]; 
				SortedRegions[iMax] = t;
    }
	}
}

void Display_Profile(void) {
	PT_T p;
	unsigned i, row;
	char buffer[32];
	
	row = 0;
	p.Y = ROW_TO_Y(row);
	for (i=0; i<NumProfileRegions; i++) {
		if (RegionCount[i] > 0) {
			p.X = COL_TO_X(0);
			p.Y = ROW_TO_Y(row);
			snprintf(buffer, TFT_MAX_COLS, "%4d %s", RegionCount[i], RegionTable[i].Name);
			TFT_Text_PrintStr(&p, buffer);
			row++;
			if (row >= TFT_MAX_ROWS) {
				row = 0;
				TFT_Erase();
			}
		}
	}
}

