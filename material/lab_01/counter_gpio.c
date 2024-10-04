#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define segment_base_address 0xFF200020
#define offset 8
#define button_up_address 0xFF200050
#define button_down_address 0xFF200060

static uint32_t number_representation_7segment[10] = { 0x3F, 0x06, 0x5B, 0x4F,
						       0x66, 0x6D, 0x7D, 0x07,
						       0x7F, 0x6F };

void set7Segment(uint32_t *base_address, unsigned value_to_print,
		 unsigned segment_number);

uint32_t get_7seg_representation(int value);

int main(void)
{
	int value_to_print = 0;
	int *p_value_to_print = &value_to_print;

	int old_value_down = 0;
	int old_value_up = 0;

	uint32_t *button_up = (uint32_t *)button_up_address;
	uint32_t *button_down = (uint32_t *)button_down_address;

	while (true) {
		uint32_t new_value_up = *button_up;
		uint32_t new_value_down = *button_down;

		if (new_value_up && !old_value_up) {
			if (*p_value_to_print < 99) {
				(*p_value_to_print)++;
			}
		}

		if (new_value_down && !old_value_down) {
			if (*p_value_to_print > 0) {
				(*p_value_to_print)--;
			}
		}

		old_value_up = new_value_up;
		old_value_down = new_value_down;

		unsigned unit = *p_value_to_print % 10;
		unsigned diz = *p_value_to_print / 10;

		set7Segment((uint32_t *)segment_base_address, unit, 0);
		set7Segment((uint32_t *)segment_base_address, diz, 1);
	}

	return 0;
}

void set7Segment(uint32_t *base_address, unsigned value_to_print,
		 unsigned segment_number)
{
	int value = get_7seg_representation(value_to_print);
	uint32_t adress = base_address + (value << (offset * segment_number))
}

void get_7seg_representation(int value)
{
	return number_representation_7segment[value];
}