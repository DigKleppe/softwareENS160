
#ifndef LEDTASK_H

#define LEDTASK_H

#include <led_strip.h>

typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} rgb_t;

#define COLORS_TOTAL (sizeof(colors) / sizeof(rgb_t))

typedef enum {
	COLOR_WHITE,
	COLOR_BLUE,
    COLOR_GREEN,
	COLOR_RED,
	COLOR_YELLOW,
	COLOR_PURPLE,
	COLOR_CYANE,
	COLOR_OFF
}LEDcolor_t;

extern LEDcolor_t onBoardColor; // leds on ledprint
extern bool onBoardFlash;


void startLEDs();
#endif
