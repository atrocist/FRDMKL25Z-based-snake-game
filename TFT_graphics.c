#include "TFT_LCD.h"

#define STEP 8

void Graphics_Test(void) {
	int x, y;
	PT_T p1, p2;
	COLOR_T c;
	
	p1.X = TFT_WIDTH/2;
	p1.Y = TFT_HEIGHT/2;

	c.R = 100;
	c.G = 100;
	c.B = 255;
	p2.Y = 0;
	for (x=0; x<TFT_WIDTH; x += STEP) {
		p2.X = x;
		TFT_Draw_Line(&p1, &p2, &c);
	}

	c.G = 100;
	c.B = 100;
	c.R = 255;
	p2.X = TFT_WIDTH-1;
	for (y=0; y<TFT_HEIGHT; y += STEP) {
		p2.Y = y;
		TFT_Draw_Line(&p1, &p2, &c);
	}

	c.R = 100;
	c.B = 100;
	c.G = 255;
	p2.Y = TFT_HEIGHT-1;
	for (x=0; x<TFT_WIDTH; x += STEP) {
		p2.X = x;
		TFT_Draw_Line(&p1, &p2, &c);
	}

	c.R = 200;
	c.B = 100;
	c.G = 255;
	p2.X = 0;
	for (y=0; y<TFT_HEIGHT; y += STEP) {
		p2.Y = y;
		TFT_Draw_Line(&p1, &p2, &c);
	}
}

void TFT_Draw_Line(PT_T * p1, PT_T * p2, COLOR_T * color)
{
	PT_T p;
  int Temp, AdjUp, AdjDown, ErrorTerm, XAdvance, XDelta, YDelta;	 
  int WholeStep, InitialPixelCount, FinalPixelCount, i,j, RunLength;
  int XStart;
  int YStart;
  int XEnd;
  int YEnd;

  XStart = p1->X;
  YStart = p1->Y;
  XEnd = p2->X;
  YEnd = p2->Y;

  /* We'll always draw top to bottom, to reduce the number of cases we have to
     ** handle, and to make lines between the same endpoints draw the same pixels
  */
  if (YStart > YEnd)	
    {
      Temp = YStart;
      YStart = YEnd;
      YEnd = Temp;
      Temp = XStart;
      XStart = XEnd;
      XEnd = Temp;
    }

  /* Figure out whethere we're going left or right, and how far we're going
  ** horizontally  */
  XDelta = XEnd - XStart;
  if (XDelta < 0) {
      XAdvance = -1;
      XDelta = -XDelta;
	} else {
      XAdvance = 1;
   }

  /* Figure out how far we're going vertically */
  YDelta = YEnd - YStart;

  /* Special-case horizontal, vertical, and diagonal lines, for speed and
  ** to avoid nasty boundary conditions and division by 0
  */

	p.X = XStart;
	p.Y = YStart;
	 
  /* Vertical Line case */
  if (XDelta == 0) {
    /* Vertical line */
    for (i = 0; i <= YDelta; i++) {
				p.Y++;
				TFT_Plot_Pixel(&p, color);
     }
    return;
  }
  
  /* Horizontal Line Case */
  if (YDelta == 0) {
    /* Horizontal line */
    for (i = 0; i <= XDelta; i++) {
				TFT_Plot_Pixel(&p, color);
				p.X += XAdvance;
    }  
    return;
  }
   
  /* Diagonal Case */
  if (XDelta == YDelta) {
    /* Diagonal line */
    for (i = 0; i <= XDelta; i++)
      {
				TFT_Plot_Pixel(&p, color);
				p.X += XAdvance;
				p.Y++;
      }
    return;
  }
   
   
  /* Determine whether the line is X or Y major, and handle accordingly */
  if (XDelta >= YDelta) {
    /* X major line */
    /* Minimum # of pixels in a run in this line */
    WholeStep = XDelta / YDelta;													/* DIV */
    
    /* Error term adjust eacn time Y steps by 1; used to tel when one
    ** extra pixel should be drawn as part of a run, to account for
    ** fractional steps along the X axis per 1-pixel steps along Y
    */
    AdjUp = (XDelta % YDelta) * 2;													/* DIV */
    
    /* Error term adjust when the error term turns over, used to factor
    ** out the X step made at that time
    */
    AdjDown = YDelta * 2;
    
    /* Initial error term; reflects an initial step of 0.5 along the Y axis 
     */
    ErrorTerm = (XDelta % YDelta) - (YDelta * 2);											/* DIV */
    
    /* The initial and last runs are partial, because Y advancse only 0.5
    ** for these runs, rather than 1.  Divide one full run, plus the
    ** initial pixel, between the initial and last runs.
    */
    InitialPixelCount = (WholeStep / 2) + 1;
    FinalPixelCount = InitialPixelCount;
    
    /* If the basic run length is even and there's no fractional
    ** advance, we have one pixel that could go to either the initial
    ** or last partial run, which we'll arbitrarily allocate to the
    ** last run
    */
    if ((AdjUp == 0) && ((WholeStep & 0x01) == 0))
      InitialPixelCount--;
    
    /* If there's an odd number of pixels per run, we have 1 pixel that can't
       ** be allocated to either the initial run or last partial run, so we'll add 0.5
       ** to error term so this pixel will be handled by the normal full-run loop
    */
    if ((WholeStep & 0x01) != 0)
      ErrorTerm += YDelta;
    
    /* Draw the first, partial run of pixels */
#ifdef USE_DRAW_LINE_FUNC
    Line_DrawHorizontalRun(pThis, &XStart, &YStart, XAdvance, 
			   InitialPixelCount, pThis->PenColor);
#else
    for (j = 0; j < InitialPixelCount; j++)
      {
				TFT_Plot_Pixel(&p, color);
				p.X += XAdvance;
      }
    p.Y++;
#endif
    
    /* Draw all full runs */
    for (i = 0; i < (YDelta - 1); i++)
      {
				RunLength = WholeStep; /* run is at least this long */
				/* Advance the error term and add an extra pixel if the error
				** term so indicates
				*/
				if ((ErrorTerm += AdjUp) > 0)
					{
						RunLength++;
						ErrorTerm -= AdjDown; /* reset the error term */
					}
	
				/* Draw this scan line's run */
#ifdef USE_DRAW_LINE_FUNC
				Line_DrawHorizontalRun(pThis, &XStart, &YStart, XAdvance, RunLength, 
			       pThis->PenColor);
#else
				for (j = 0; j < RunLength; j++) {
					TFT_Plot_Pixel(&p, color);
					p.X += XAdvance;
				}
				p.Y++;
#endif
				}
    
    /* Draw the final run of pixels */
#ifdef USE_DRAW_LINE_FUNC
    Line_DrawHorizontalRun(pThis, &XStart, &YStart, XAdvance, 
			   FinalPixelCount, pThis->PenColor);
#else
    for (j = 0; j < FinalPixelCount; j++)
      {
				TFT_Plot_Pixel(&p, color);
				p.X += XAdvance;
      }
    p.Y++;
#endif
    
    return;
  } else {
    /* Y major line */
    
    /* Minimum # of pixels in a run in this line */
    WholeStep = YDelta / XDelta;										/* DIV */
    
    /* Error term adjust each time X steps by 1; used to tell when 1 extra
    ** pixel should be drawn as part of a run, to account for
    ** fractional steps along the Y axis per 1-pixel steps along X
    */
    AdjUp = (YDelta % XDelta) * 2;										/* DIV */
    
    /* Error term adjust when the error term turns over, used to factor
    ** out the Y step made at that time
    */
    AdjDown = XDelta * 2;
    
    /* Initial error term; reflects initial step of 0.5 along the X axis */
    ErrorTerm = (YDelta % XDelta) - (XDelta * 2);								/* DIV */
    
    /* The initial and last runs are partial, because X advances only 0.5
    ** for these runs, rather than 1.  Divide ony full run, plus the
    ** initial pixel, between the initial and last runs
    */
    
    InitialPixelCount = (WholeStep / 2) + 1;
    FinalPixelCount = InitialPixelCount;
    
    /* If the basic run length is even and there's no fractional
      ** advance, we have one pixel that could go to either the initial
      ** or last partial run, which we'll arbitrarily allocate to the
      ** last run
      */
      if ((AdjUp == 0) && ((WholeStep & 0x01) == 0))
	{
	  InitialPixelCount--;
	}
      
      /* If there's an odd number of pixels per run, we have 1 pixel that 
	 can't
	 ** be allocated to either the initial run or last partial run, so we'll 
	 add 0.5
	 ** to error term so this pixel will be handled by the normal full-run 
	 loop
      */
      if ((WholeStep & 0x01) != 0)
	{
	  ErrorTerm += XDelta;
	}
      
      /* Draw the first, partial run of pixels */
#ifdef USE_DRAW_LINE_FUNC
      Line_DrawVerticalRun(pThis, &XStart, &YStart, XAdvance, 
			   InitialPixelCount, pThis->PenColor);
#else
      for (j = 0; j < InitialPixelCount; j++) {
				TFT_Plot_Pixel(&p, color);
				p.Y++;
			}
      /* Update x,y position */
      p.X += XAdvance;
#endif
      
      /* Draw all full runs */
      for (i = 0; i < (XDelta - 1); i++) {
				RunLength = WholeStep; /* run is at least this long */
	  
				/* Advance the error term and add an extra pixel if the error
				** term so indicates
				*/
	  if ((ErrorTerm += AdjUp) > 0)
	    {
	      RunLength++;
	      ErrorTerm -= AdjDown; /* reset the error term */
	    }
	  
	  /* Draw this scan line's run */
#ifdef USE_DRAW_LINE_FUNC
	  Line_DrawVerticalRun(pThis, &XStart, &YStart, XAdvance, RunLength, 
			       pThis->PenColor);
#else
	  for (j = 0; j < RunLength; j++)
	    {
				TFT_Plot_Pixel(&p, color);
	      p.Y++;
	    }
	  /* Update x,y position */
	  p.X += XAdvance;
#endif
	  
	}
      
      /* Draw the final run of pixels */
#ifdef USE_DRAW_LINE_FUNC
      Line_DrawVerticalRun(pThis, &XStart, &YStart, XAdvance, 
			   FinalPixelCount, pThis->PenColor);
#else
      for (j= 0; j < FinalPixelCount; j++)
			{
				TFT_Plot_Pixel(&p, color);
				p.Y++;
			}
      /* Update x,y position */
      p.X += XAdvance;
#endif    
      return;
    }
}
