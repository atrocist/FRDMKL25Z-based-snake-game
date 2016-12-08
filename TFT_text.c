#include "font.h"
#include "TFT_LCD.h"
#include "tasks.h"

const uint8_t * font;
FONT_HEADER_T * font_header;
GLYPH_INDEX_T * glyph_index; 

COLOR_T fg, bg;

uint8_t G_TFT_char_width, G_TFT_char_height;

const uint8_t * fonts[] = {Lucida_Console8x13, Lucida_Console12x19}; // Lucida_Console20x31};
const uint8_t char_widths[] = {8, 12, 20};
const uint8_t char_heights[] = {13, 19, 31};

void TFT_Text_Set_Colors(COLOR_T * foreground, COLOR_T * background) {
	fg.R = foreground->R;
	fg.G = foreground->G;
	fg.B = foreground->B;
	bg.R = background->R;
	bg.G = background->G;
	bg.B = background->B;
}

void TFT_Erase(void) {
	TFT_Fill_Buffer(&bg);
}

void TFT_Text_Init(uint8_t font_num) {
	
#if 0  // Code for multiple fonts not working yet
	font = fonts[font_num];
	G_TFT_char_width = char_widths[font_num];
	G_TFT_char_height = char_heights[font_num];
#else // This code is working
	font = Lucida_Console12x19;
	G_TFT_char_width = 12;
	G_TFT_char_height = 19;
#endif
	
	font_header = (FONT_HEADER_T *) font;
	glyph_index = (GLYPH_INDEX_T *) (font + sizeof(FONT_HEADER_T));
	
	// Set default FG and BG colors
	fg.R = 255;
	fg.G = 255;
	fg.B = 0;

	bg.R = 0;
	bg.G = 0;
	bg.B = 0;
}

void TFT_Text_PrintChar(PT_T * pos, char ch) {
	uint8_t glyph_index_entry;
	const uint8_t * glyph_data; // start of the data
	PT_T end_pos;
	COLOR_T * pixel_color;
	uint8_t bitmap_byte;
	uint8_t glyph_width, x_bm;
	uint32_t offset;
	uint32_t row, col, num_pixels;
	
	glyph_index_entry = ch - font_header->FirstChar;
	glyph_width = glyph_index[glyph_index_entry].Width;
	offset = glyph_index[glyph_index_entry].Offset;
	glyph_data = &(font[offset]);
	
	end_pos.X = pos->X+CHAR_WIDTH-1;
	end_pos.Y = pos->Y+CHAR_HEIGHT-1;
	TFT_Start_Rectangle(pos, &end_pos); 
	
	for (row = 0; row < CHAR_HEIGHT; row++) {
		x_bm = 0;
		do {
			bitmap_byte = *glyph_data;
			if (bitmap_byte == 0x00) {
				num_pixels = MIN(8,glyph_width - x_bm);
				TFT_Write_Rectangle_Pixel(&bg, num_pixels);
				x_bm += num_pixels;	
			} else if (bitmap_byte == 0xff) {
				num_pixels = MIN(8,glyph_width - x_bm);
				TFT_Write_Rectangle_Pixel(&fg, num_pixels);
				x_bm += num_pixels;	
			} else {
				col = 0;
				num_pixels = 0;
				if ((bitmap_byte & 0x7f) == 0) {
					num_pixels = MIN(7,glyph_width - x_bm);
					TFT_Write_Rectangle_Pixel(&bg, num_pixels);
				} else if ((bitmap_byte & 0x7f) == 0x7f) {
					num_pixels = MIN(7,glyph_width - x_bm);
					TFT_Write_Rectangle_Pixel(&fg, num_pixels);
				} else if ((bitmap_byte & 0x3f) == 0) {
					num_pixels = MIN(6,glyph_width - x_bm);
					TFT_Write_Rectangle_Pixel(&bg, num_pixels);
				} else if ((bitmap_byte & 0x3f) == 0x3f) {
					num_pixels = MIN(6,glyph_width - x_bm);
					TFT_Write_Rectangle_Pixel(&fg, num_pixels);
				} else if ((bitmap_byte & 0x1f) == 0) {
					num_pixels = MIN(5,glyph_width - x_bm);
					TFT_Write_Rectangle_Pixel(&bg, num_pixels);
				} else if ((bitmap_byte & 0x1f) == 0x1f) {
					num_pixels = MIN(5,glyph_width - x_bm);
					TFT_Write_Rectangle_Pixel(&fg, num_pixels);
				} else if ((bitmap_byte & 0x0f) == 0) {
					num_pixels = MIN(4,glyph_width - x_bm);
					TFT_Write_Rectangle_Pixel(&bg, num_pixels);
				} else if ((bitmap_byte & 0x0f) == 0x0f) {
					num_pixels = MIN(4,glyph_width - x_bm);
					TFT_Write_Rectangle_Pixel(&fg, num_pixels);
				}
				if (num_pixels > 0) {
					x_bm += num_pixels;	
					col += num_pixels;
					bitmap_byte >>= num_pixels;
				}
				for (; col < 8; col++) {
					if (bitmap_byte & 0x01) // if pixel is to be set
						pixel_color = &fg;
					else
						pixel_color = &bg;
					TFT_Write_Rectangle_Pixel(pixel_color, 1);
					bitmap_byte >>= 1;
					x_bm++;
					if (x_bm >= glyph_width)
						break;
				}
			}
			glyph_data++;
		} while (x_bm < glyph_width);
		if (x_bm < CHAR_WIDTH) {
			// fill in rest of cell with background color for narrow glyphs
			TFT_Write_Rectangle_Pixel(&bg, CHAR_WIDTH - x_bm);	
		}
	}
}

void TFT_Text_PrintStr(PT_T * pos, char * str) {
	while (*str) {
		TFT_Text_PrintChar(pos, *str);
		str++;
		pos->X += CHAR_WIDTH; // forces monospacing for fonts
	}
}

void TFT_Text_PrintStr_RC( uint8_t  row, uint8_t  col, char  *str )
{
	PT_T pos;
	pos.X = COL_TO_X( col );
	pos.Y = ROW_TO_Y( row );
	
	get_stack_depth(MAX_STACK);
	
	while( *str )
	{
		TFT_Text_PrintChar( &pos, *str );
		str++;
		pos.X += CHAR_WIDTH;       /*     forces monospacing for fonts               */
	}
}
