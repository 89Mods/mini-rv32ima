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
int fds[NUM_LEDS];
struct gpiohandle_request* rqs;
struct gpiohandle_data* datas;

void init_leds() {
	for(uint8_t i = 0; i < NUM_LEDS; i++) {
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
		if(info.lines != 25) printf("Weird number of lines on %s (%u): probably not an LED\n", info.name, info.lines);
		for(int j = 0; j < 25; j++) rqs[i].lineoffsets[j] = j;
		rqs[i].lines = 25;
		rqs[i].flags = GPIOHANDLE_REQUEST_OUTPUT;
		ret = ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, rqs + i);
		if(ret == -1) {
			printf("Unable to line handle from ioctl : %s\n", strerror(errno));
			close(fd);
			exit(1);
		}
		fds[i] = fd;
	}
}

void deinit_leds() {
	for(int i = 0; i < NUM_LEDS; i++) close(fds[i]);
}

void led_push(uint8_t led_idx) {
	int ret = ioctl(rqs[led_idx].fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, datas + led_idx);
	if(ret == -1) {
		printf("Unable to set line value using ioctl : %%s", strerror(errno));
		deinit_leds();
		exit(1);
	}
}

void set_led_color(uint8_t led_idx, uint32_t color) {
	for(int i = 0; i < 24; i++) {
		datas[led_idx].values[i] = (color & 1) != 0;
		color >>= 1;
	}
	datas[led_idx].values[24] = 1;
	led_push(led_idx);
}

void release_led(uint8_t led_idx) {
	datas[led_idx].values[24] = 0;
	led_push(led_idx);
}

void delay_ms(uint32_t ms) {
	uint64_t ns = ms * 100000;
	nanosleep((const struct timespec[]){{ns / 1000000000, ns % 1000000000}}, NULL);
}

int main(int argc, char ** argv) {
	rqs = (struct gpiohandle_request*)malloc(NUM_LEDS * sizeof(struct gpiohandle_request));
	datas = (struct gpiohandle_data*)malloc(NUM_LEDS * sizeof(struct gpiohandle_data));
	if(!rqs || !datas) {
		printf("Malloc fail\n");
		return 1;
	}
	init_leds();
	
	printf("\n /$$$$$$$$ /$$   /$$  /$$$$$$  /$$       /$$$$$$ /$$   /$$\n|__  $$__/| $$  | $$ /$$__  $$| $$      |_  $$_/| $$$ | $$\n   | $$   | $$  | $$| $$  \\ $$| $$        | $$  | $$$$| $$\n   | $$   | $$$$$$$$| $$  | $$| $$        | $$  | $$ $$ $$\n   | $$   | $$__  $$| $$  | $$| $$        | $$  | $$  $$$$\n   | $$   | $$  | $$| $$  | $$| $$        | $$  | $$\\  $$$\n   | $$   | $$  | $$|  $$$$$$/| $$$$$$$$ /$$$$$$| $$ \\  $$\n   |__/   |__/  |__/ \\______/ |________/|______/|__/  \\__/\n\n");
	delay_ms(400);
	printf("ALL YOUR LEDS ARE BELONG TO US\n\n");
	delay_ms(1000);
	for(int i = 0; i < NUM_LEDS; i++) set_led_color(i, 0);
	delay_ms(2000);
	for(int i = 0; i < NUM_LEDS; i++) {
		if(i >= EFLED_EFBAR_OFFSET && i < EFLED_EFBAR_OFFSET + EFLED_EFBAR_NUM) {
			if((i & 1) == 0) set_led_color(i, 0x29d1ff);
			else set_led_color(i, 0xa4ecff);
		}else set_led_color(i, 0x10FF40);
	}
	delay_ms(3333);
	
	/*struct gpiohandle_data data;
	for(int i = 0; i < 25; i++) data.values[i] = 0;
	ret = ioctl(rq.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
	if(ret == -1) {
		printf("Unable to set line value using ioctl : %%s", strerror(errno));
		close(fd);
		return 1;
	}*/
	
	uint32_t curr_col = 0x0000FF;
	uint32_t loop_ctr = 0;
	while(1) {
		for(uint8_t i = 0; i < NUM_LEDS; i++) set_led_color(i, curr_col);
		//set_led_color(EFLED_DRAGON_EAR_BOTTOM_IDX, curr_col);
		curr_col = curr_col == 0x0000FF ? 0x00FF00 : (curr_col == 0x00FF00 ? 0xFF0000 : 0x0000FF);
		delay_ms(1000);
		loop_ctr++;
		if((loop_ctr % 100) == 0) {
			printf("%u loops\r\n", loop_ctr);
		}
	}
	
	return 0;
}
