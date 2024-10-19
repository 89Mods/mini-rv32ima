#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <linux/gpio.h>
#include <time.h>
#include <math.h>

#define NUM_LEDS 17
#define EFLED_DRAGON_NUM 6
#define EFLED_EFBAR_NUM 11

#define EFLED_DARGON_OFFSET 0
#define EFLED_EFBAR_OFFSET 6

#define EFLED_DRAGON_NOSE_IDX 0
#define EFLED_DRAGON_MUZZLE_IDX 1
#define EFLED_DRAGON_EYE_IDX 2
#define EFLED_DRAGON_CHEEK_IDX 3
#define EFLED_DRAGON_EAR_BOTTOM_IDX 4
#define EFLED_DRAGON_EAR_TOP_IDX 5

char* dev_name = "/dev/gpiochip00";
int fds[NUM_LEDS+1];
struct gpiohandle_request* rqs;
struct gpiohandle_data* datas;

void init_leds() {
	for(uint32_t i = 0; i != NUM_LEDS+1; i++) {
		dev_name[13] = i < 10 ? '0' + i % 10 : '0' + i / 10;
		dev_name[14] = i < 10 ? 0 : '0' + i % 10;
		int fd = open(dev_name, O_RDONLY);
		if(fd < 0) {
			printf("Unabled to open %s: %s\n", dev_name, strerror(errno));
			exit(1);
		}
		struct gpiochip_info info;
		int ret = ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &info);
		if(ret == -1) {
			printf("Unable to get chip info from ioctl: %s\n", strerror(errno));
			close(fd);
			exit(1);
		}
		if(i == NUM_LEDS) {
			rqs[i].lines = 3;
			rqs[i].lineoffsets[0] = 3;
			rqs[i].lineoffsets[1] = 4;
			rqs[i].lineoffsets[2] = 5;
			datas[i].values[1] = datas[i].values[2] = 0;
			rqs[i].flags = GPIOHANDLE_REQUEST_OUTPUT;
		}else {
			if(info.lines != 25) printf("Weird number of lines on %s (%u): probably not an LED\n", info.name, info.lines);
			for(int j = 0; j < 25; j++) rqs[i].lineoffsets[j] = j;
			rqs[i].lines = 25;
			rqs[i].flags = GPIOHANDLE_REQUEST_OUTPUT;
		}
		ret = ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, rqs + i);
		if(ret == -1) {
			printf("Unable to line handle from ioctl : %s\n", strerror(errno));
			close(fd);
			exit(1);
		}
		fds[i] = fd;
	}
}

void deinit_leds();
void gpio_push(uint8_t led_idx) {
	int ret = ioctl(rqs[led_idx].fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, datas + led_idx);
	if(ret == -1) {
		printf("Unable to set line value using ioctl : %s", strerror(errno));
		deinit_leds();
		exit(1);
	}
}

void release_led(uint8_t led_idx) {
	if(led_idx >= NUM_LEDS) return;
	datas[led_idx].values[24] = 0;
	gpio_push(led_idx);
}

void disable_watchdog();
void deinit_leds() {
	disable_watchdog();
	for(int i = 0; i < NUM_LEDS; i++) {
		release_led(i);
		close(fds[i]);
	}
}

void set_led_color(uint8_t led_idx, uint32_t color) {
	if(led_idx >= NUM_LEDS) return;
	for(int i = 0; i < 24; i++) {
		datas[led_idx].values[i] = (color & 1) != 0;
		color >>= 1;
	}
	datas[led_idx].values[24] = 1;
	gpio_push(led_idx);
}

void leds_sync() {
	datas[NUM_LEDS].values[0] = 1;
	gpio_push(NUM_LEDS);
	datas[NUM_LEDS].values[0] = 0;
	gpio_push(NUM_LEDS);
}

void delay_ms(uint32_t ms) {
	uint64_t ns = ms * 100000;
	nanosleep((const struct timespec[]){{ns / 1000000000, ns % 1000000000}}, NULL);
}

void enable_watchdog() {
	datas[NUM_LEDS].values[1] = 1;
	datas[NUM_LEDS].values[2] = 0;
	gpio_push(NUM_LEDS);
}

void disable_watchdog() {
	datas[NUM_LEDS].values[1] = datas[NUM_LEDS].values[2] = 0;
	gpio_push(NUM_LEDS);
}

void pet_watchdog() {
	datas[NUM_LEDS].values[2] = 1;
	gpio_push(NUM_LEDS);
	datas[NUM_LEDS].values[2] = 0;
	gpio_push(NUM_LEDS);
}

uint32_t hsv_to_rgb(float h, float s, float v){
	float hue = (float)h / 255.0f;
	float saturation = (float)s / 255.0f;
	float brightness = (float)v / 255.0f;
	uint32_t r = 0, g = 0, b = 0;
	if(saturation == 0){
		r = g = b = (int) (brightness * 255.0 + 0.5);
	}else{
		float h = (hue - (float)floor(hue)) * 6.0;
		float f = h - (float)floor(h);
		float p = brightness * (1.0 - saturation);
		float q = brightness * (1.0 - saturation * f);
		float t = brightness * (1.0 - (saturation * (1.0 - f)));
		switch ((uint32_t) h) {
		case 0:
			r = (uint32_t) (brightness * 255.0 + 0.5);
			g = (uint32_t) (t * 255.0 + 0.5);
			b = (uint32_t) (p * 255.0 + 0.5);
			break;
		case 1:
			r = (uint32_t) (q * 255.0 + 0.5);
			g = (uint32_t) (brightness * 255.0 + 0.5);
			b = (uint32_t) (p * 255.0 + 0.5);
			break;
		case 2:
			r = (uint32_t) (p * 255.0 + 0.5);
			g = (uint32_t) (brightness * 255.0 + 0.5);
			b = (uint32_t) (t * 255.0 + 0.5);
			break;
		case 3:
			r = (uint32_t) (p * 255.0 + 0.5);
			g = (uint32_t) (q * 255.0 + 0.5);
			b = (uint32_t) (brightness * 255.0 + 0.5);
			break;
		case 4:
			r = (uint32_t) (t * 255.0 + 0.5);
			g = (uint32_t) (p * 255.0 + 0.5);
			b = (uint32_t) (brightness * 255.0 + 0.5);
			break;
		case 5:
			r = (uint32_t) (brightness * 255.0 + 0.5);
			g = (uint32_t) (p * 255.0 + 0.5);
			b = (uint32_t) (q * 255.0 + 0.5);
			break;
		}
	}
	return 0xff000000 | (r << 16) | (g << 8) | (b << 0);
}

void the_colors(uint8_t delay) {
	for(int i = 0; i < NUM_LEDS; i++) {
		if(i >= EFLED_EFBAR_OFFSET && i < EFLED_EFBAR_OFFSET + EFLED_EFBAR_NUM) {
			if((i & 1) == 0) set_led_color(i, 0x29d1ff);
			else set_led_color(i, 0xa4ecff);
		}else set_led_color(i, 0x10FF40);
	}
	leds_sync();
	pet_watchdog();
	if(delay) delay_ms(3333);	
}

int main(int argc, char ** argv) {
	rqs = (struct gpiohandle_request*)malloc((NUM_LEDS + 1) * sizeof(struct gpiohandle_request));
	datas = (struct gpiohandle_data*)malloc((NUM_LEDS + 1) * sizeof(struct gpiohandle_data));
	if(!rqs || !datas) {
		printf("Malloc fail\n");
		return 1;
	}
	init_leds();
	enable_watchdog();
	
	printf("\n /$$$$$$$$ /$$   /$$  /$$$$$$  /$$       /$$$$$$ /$$   /$$\n|__  $$__/| $$  | $$ /$$__  $$| $$      |_  $$_/| $$$ | $$\n   | $$   | $$  | $$| $$  \\ $$| $$        | $$  | $$$$| $$\n   | $$   | $$$$$$$$| $$  | $$| $$        | $$  | $$ $$ $$\n   | $$   | $$__  $$| $$  | $$| $$        | $$  | $$  $$$$\n   | $$   | $$  | $$| $$  | $$| $$        | $$  | $$\\  $$$\n   | $$   | $$  | $$|  $$$$$$/| $$$$$$$$ /$$$$$$| $$ \\  $$\n   |__/   |__/  |__/ \\______/ |________/|______/|__/  \\__/\n\n");
	delay_ms(400);
	printf("ALL YOUR LEDS ARE BELONG TO US\n\n");
	pet_watchdog();
	delay_ms(100);
	the_colors(0);
	
	/*struct gpiohandle_data data;
	for(int i = 0; i < 25; i++) data.values[i] = 0;
	ret = ioctl(rq.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
	if(ret == -1) {
		printf("Unable to set line value using ioctl : %%s", strerror(errno));
		close(fd);
		return 1;
	}*/
	
	uint32_t* colors_lut = (uint32_t*)malloc(256 * 128 * sizeof(uint32_t));
	for(uint16_t i = 0; i < 128; i++) {
		for(uint16_t j = 0; j < 256; j++) {
			colors_lut[i * 256 + j] = hsv_to_rgb(j, i + 128, 255);
		}
		pet_watchdog();
	}
	
	uint32_t curr_col = 0x0000FF;
	uint32_t loop_ctr = 0;
	uint8_t curr_sat = 255;
	uint8_t sat_dir = 0;
	uint16_t sat_cooldown = 512;
	while(1) {
		pet_watchdog();
		for(uint8_t i = 0; i < NUM_LEDS; i++) {
			uint8_t hue = (uint8_t)((float)i / (float)(NUM_LEDS) * 255.0f);
			hue += (uint8_t)(loop_ctr * 3);
			set_led_color(i, colors_lut[(curr_sat - 128) * 256 + hue]);
		}
		leds_sync();
		delay_ms(22);
		loop_ctr++;
		if(sat_cooldown == 0) {
			if(sat_dir) curr_sat++;
			else curr_sat--;
			if(curr_sat == 255 || curr_sat == 128) sat_dir = !sat_dir;
			if(curr_sat == 255) sat_cooldown = 500;
		}else sat_cooldown--;
		if((loop_ctr % 1000) == 0) {
			printf("%u loops\r\n", loop_ctr);
		}
	}
	
	deinit_leds();
	free(colors_lut);
	return 0;
}
