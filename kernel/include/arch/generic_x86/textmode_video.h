
#pragma once

#include <common_defs.h>
#include <hal.h>


/* Hardware text mode color constants. */
enum vga_color {
   COLOR_BLACK = 0,
   COLOR_BLUE = 1,
   COLOR_GREEN = 2,
   COLOR_CYAN = 3,
   COLOR_RED = 4,
   COLOR_MAGENTA = 5,
   COLOR_BROWN = 6,
   COLOR_LIGHT_GREY = 7,
   COLOR_DARK_GREY = 8,
   COLOR_LIGHT_BLUE = 9,
   COLOR_LIGHT_GREEN = 10,
   COLOR_LIGHT_CYAN = 11,
   COLOR_LIGHT_RED = 12,
   COLOR_LIGHT_MAGENTA = 13,
   COLOR_LIGHT_BROWN = 14,
   COLOR_WHITE = 15,
};

#define make_color(fg, bg) ((fg) | (bg) << 4)
#define make_vgaentry(c, color) (((u16)c) | ((u16)color << 8))

/* Main functions */
void textmode_set_char_at(char c, u8 color, int row, int col);
void textmode_clear_row(int row_num);

/* Scrolling */
void textmode_scroll_up(u32 lines);
void textmode_scroll_down(u32 lines);
bool textmode_is_at_bottom(void);
void textmode_scroll_to_bottom(void);
void textmode_add_row_and_scroll(void);

/* Cursor management */
void textmode_move_cursor(int row, int col);
void textmode_enable_cursor(void);
void textmode_disable_cursor(void);