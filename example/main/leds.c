#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "lssl_idf_web.h"
#include "vm.h"
#include "led_syscalls.h"
#include "lssl_idf_web.h"

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define RMT_LED_STRIP_GPIO_NUM		8

#define LED_COUNT	100

static const char *TAG = "leds";

static uint8_t led_strip_pixels[LED_COUNT * 3];

static const rmt_symbol_word_t ws2812_zero = {
	.level0 = 1,
	.duration0 = 0.3 * RMT_LED_STRIP_RESOLUTION_HZ / 1000000, // T0H=0.3us
	.level1 = 0,
	.duration1 = 0.9 * RMT_LED_STRIP_RESOLUTION_HZ / 1000000, // T0L=0.9us
};

static const rmt_symbol_word_t ws2812_one = {
	.level0 = 1,
	.duration0 = 0.9 * RMT_LED_STRIP_RESOLUTION_HZ / 1000000, // T1H=0.9us
	.level1 = 0,
	.duration1 = 0.3 * RMT_LED_STRIP_RESOLUTION_HZ / 1000000, // T1L=0.3us
};

//reset defaults to 50uS
static const rmt_symbol_word_t ws2812_reset = {
	.level0 = 1,
	.duration0 = RMT_LED_STRIP_RESOLUTION_HZ / 1000000 * 50 / 2,
	.level1 = 0,
	.duration1 = RMT_LED_STRIP_RESOLUTION_HZ / 1000000 * 50 / 2,
};

static size_t encoder_callback(const void *data, size_t data_size,
							   size_t symbols_written, size_t symbols_free,
							   rmt_symbol_word_t *symbols, bool *done, void *arg)
{
	// We need a minimum of 8 symbol spaces to encode a byte. We only
	// need one to encode a reset, but it's simpler to simply demand that
	// there are 8 symbol spaces free to write anything.
	if (symbols_free < 8) {
		return 0;
	}

	// We can calculate where in the data we are from the symbol pos.
	// Alternatively, we could use some counter referenced by the arg
	// parameter to keep track of this.
	size_t data_pos = symbols_written / 8;
	uint8_t *data_bytes = (uint8_t*)data;
	if (data_pos < data_size) {
		// Encode a byte
		size_t symbol_pos = 0;
		for (int bitmask = 0x80; bitmask != 0; bitmask >>= 1) {
			if (data_bytes[data_pos]&bitmask) {
				symbols[symbol_pos++] = ws2812_one;
			} else {
				symbols[symbol_pos++] = ws2812_zero;
			}
		}
		// We're done; we should have written 8 symbols.
		return symbol_pos;
	} else {
		//All bytes already are encoded.
		//Encode the reset, and we're done.
		symbols[0] = ws2812_reset;
		*done = 1; //Indicate end of the transaction.
		return 1; //we only wrote one symbol
	}
}

QueueHandle_t progq;

void leds_change_program(const char *pgm) {
	xQueueSend(progq, pgm, portMAX_DELAY);
}

static rmt_channel_handle_t led_chan = NULL;
static rmt_encoder_handle_t simple_encoder = NULL;


static void led_task(void *args) {
	char progname[17];
	size_t pgmlen;
	uint8_t *pgm=NULL;
	lssl_vm_t *vm=NULL;

	led_syscalls_init();
	vm_error_t err={};
	while(1) {
		if (xQueueReceive(progq, progname, pdMS_TO_TICKS(10))) {
			free(pgm);
			pgm=lssl_web_get_program(progname, &pgmlen);
			if (pgm) {
				if (vm) lssl_vm_free(vm);
				vm=lssl_vm_init(pgm, pgmlen, 8192);
				if (vm) {
					lssl_vm_run_main(vm, &err);
				} else {
					printf("Couldn't init vm!\n");
					free(pgm);
					pgm=NULL;
				}
			} else {
				printf("leds: progname not found!\n");
			}
		}
		if (pgm && err.type==LSSL_VM_ERR_NONE) {
			led_syscalls_frame_start(vm, &err);
			if (err.type==LSSL_VM_ERR_NONE) {
				float t=esp_timer_get_time()/1000000.0;
				for (int i=0; i<LED_COUNT; i++) {
					led_syscalls_calculate_led(vm, i, t, &err);
					if (err.type!=LSSL_VM_ERR_NONE) break;
					led_syscalls_get_rgb(&led_strip_pixels[i*3],
										&led_strip_pixels[i*3+1],
										&led_strip_pixels[i*3+2]);
					if (err.type!=LSSL_VM_ERR_NONE) break;
				}
				const rmt_transmit_config_t tx_config = {
					.loop_count = 0, // no transfer loop
				};
				ESP_ERROR_CHECK(rmt_transmit(led_chan, simple_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
				ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
			}
		}
	}
}

void leds_init(int gpio) {
	ESP_LOGI(TAG, "Create RMT TX channel");
	rmt_tx_channel_config_t tx_chan_config = {
		.clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
		.gpio_num = gpio,
		.mem_block_symbols = 64, // increase the block size can make the LED less flickering
		.resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
		.trans_queue_depth = 4, // set the number of transactions that can be pending in the background
	};
	ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

	ESP_LOGI(TAG, "Create simple callback-based encoder");
	const rmt_simple_encoder_config_t simple_encoder_cfg = {
		.callback = encoder_callback
		//Note we don't set min_chunk_size here as the default of 64 is good enough.
	};
	ESP_ERROR_CHECK(rmt_new_simple_encoder(&simple_encoder_cfg, &simple_encoder));
	
	ESP_LOGI(TAG, "Enable RMT TX channel");
	ESP_ERROR_CHECK(rmt_enable(led_chan));
	
	progq=xQueueCreate(1, 17);
	xTaskCreate(led_task, "led", 8192, NULL, 4, NULL);
}

