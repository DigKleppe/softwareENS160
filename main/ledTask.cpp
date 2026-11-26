#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdbool.h>

#include "esp_log.h"

#include "ledTask.h"
#include "led_strip.h"
#include "wifiConnect.h"

static const char *TAG = "ledTask";

#define LED_TYPE LED_MODEL_WS2812
#define LED_GPIO1 GPIO_NUM_48 // on 48 s3 board   47 // on s3 mini board ,38 on s3 dev board espressif

#define BRIGHTNESS 100

static const rgb_t colorsOnboardLed[] = {
	{.r = 0x0f, .g = 0x0f, .b = 0x0f}, // wit
	{.r = 0x00, .g = 0x00, .b = 0x2f}, // blauw
	{.r = 0x2f, .g = 0x00, .b = 0x00}, // rood
	{.r = 0x00, .g = 0x2f, .b = 0x00}, // groen
	{.r = 0x2f, .g = 0x2f, .b = 0x00}, // geel
	{.r = 0x2f, .g = 0x00, .b = 0x2f}, // paars
	{.r = 0x00, .g = 0x2f, .b = 0x2f}, // cyaan
	{.r = 0x00, .g = 0x00, .b = 0x00}, // uit
};




esp_err_t led_strip_set_pixelColor(led_strip_handle_t strip, uint32_t index, rgb_t color) {
	int r = (color.r * BRIGHTNESS) / 100;
	int g = (color.g * BRIGHTNESS) / 100;
	int b = (color.b * BRIGHTNESS) / 100;
	return led_strip_set_pixel(strip, index, r , g, b);
}

void LEDtask(void *pvParameters) {
	LEDcolor_t c = COLOR_CYANE;
	bool flash = false;

	static led_strip_handle_t strip;
	/* LED strip initialization with the GPIO and pixels number*/

	led_strip_config_t strip_config = {
		.strip_gpio_num = LED_GPIO1, // on s3 board
		.max_leds = 2,
		.led_model = LED_TYPE, // set the LED strip model
		.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_RGB,
		.flags = {false},				// set to 1 if the output signal need to be inverted
	};
	led_strip_rmt_config_t rmt_config = {
		.clk_src = RMT_CLK_SRC_DEFAULT,	   // set the RMT clock source, if not set, default to APB_CLK
		.resolution_hz = 10 * 1000 * 1000, // 10MHz
		.mem_block_symbols = 64,		   // set the number of symbols that can be stored in one RMT channel, if not set, default to 48
		.flags = {false},
	};

	ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &strip));
	/* Set all LED off to clear all pixels */
	led_strip_clear(strip);

	// while (1) {
	// 	for (int step = 0; step < 4; step++) {
	// 		ESP_LOGI(TAG, "step: %d", step);
	// 		switch (step) {
	// 		case 0:
	// 			led_strip_set_pixelColor(strip, 0, colorsOnboardLed[COLOR_OFF]);
	// 			break;
	// 		case 1:
	// 			led_strip_set_pixelColor(strip, 0, colorsOnboardLed[COLOR_RED]);
	// 			break;
	// 		case 2:
	// 			led_strip_set_pixelColor(strip, 0, colorsOnboardLed[COLOR_GREEN]);
	// 			break;
	// 		case 3:
	// 			led_strip_set_pixelColor(strip, 0, colorsOnboardLed[COLOR_BLUE]);
	// 			break;
	// 		}
	// 		led_strip_refresh(strip);
	// 		vTaskDelay(pdMS_TO_TICKS(1000));
	// 	}
	// }

	while (1) {
		switch ((int)connectStatus) {
		case CONNECTING:
			ESP_LOGI(TAG, "CONNECTING");
			c = COLOR_RED;
			break;

		case WPS_ACTIVE:
			ESP_LOGI(TAG, "WPS_ACTIVE");
			flash = true;
			c = COLOR_BLUE;
			break;

		case IP_RECEIVED:
		case CONNECT_READY:
			//     ESP_LOGI(TAG, "IP_RECEIVED");
			flash = false;
			c = COLOR_GREEN;
			break;

		case CONNECTED:
			ESP_LOGI(TAG, "CONNECTED");
			c = COLOR_YELLOW;
			break;

		case CONNECT_AP:
			flash = false;
			c = COLOR_PURPLE;
			break;

		case CONNECT_READY_AP:
			c = COLOR_GREEN;
			break;

		default:
			ESP_LOGI(TAG, "default");
			c = COLOR_OFF;
			break;
		}

		led_strip_set_pixelColor(strip, 0, colorsOnboardLed[(int)c]);
		led_strip_refresh(strip);

		if (flash) {
			vTaskDelay(pdMS_TO_TICKS(300));
			led_strip_set_pixelColor(strip, 0, colorsOnboardLed[COLOR_OFF]);
			led_strip_refresh(strip);
			vTaskDelay(pdMS_TO_TICKS(300));
		} else
			vTaskDelay(pdMS_TO_TICKS(1000));
	}
}


void startLEDs() { xTaskCreate(LEDtask, "LEDtask", 3000, NULL, tskIDLE_PRIORITY, NULL); }