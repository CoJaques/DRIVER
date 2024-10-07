#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define BASE_ADDRESS   0xFF200000
#define LEDS_OFFSET    0x0
#define SEGMENT_OFFSET 0x20
#define BUTTON_OFFSET  0x50
#define OFFSET	       0x8
#define KEY_0_MASK     (1 << 0)
#define KEY_1_MASK     (1 << 1)
#define KEY_2_MASK     (1 << 2)
#define KEY_3_MASK     (1 << 3)
#define MIN	       0
#define MAX	       99

static const uint32_t number_representation_7segment[] = {
	0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

void set_7_segment(uint32_t *base_address, unsigned value_to_print)
{
	uint8_t unit = value_to_print % 10;
	uint8_t diz = value_to_print / 10;

	uint32_t value = number_representation_7segment[unit] |
			 (number_representation_7segment[diz] << OFFSET);

	*base_address = value;

	printf("Value: %d Setting 7-segment value to 0x%X address to %p\n",
	       value_to_print, value, base_address);
}

void set_leds(uint32_t *base_address, unsigned value_to_print)
{
	// turn on led 1 to 9 depending on the unit of the counter.
	// 0 is off.
	*base_address = value > 0 ? (1 << (value - 1)) : 0;
}

void clear_7_segment(uint32_t *addr)
{
	*addr = 0x0;
}

// Return the state of the key if the value was changed since the last call (edge detection)
int get_key_state(uint8_t *addr)
{
	static uint8_t last_state = 0;
	uint8_t current_state = *addr;

	int key_pressed = (last_state ^ current_state) & current_state;
	last_state = current_state;
	return key_pressed;
}

int main(void)
{
	int fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) {
		printf("Error opening /dev/uio0\n");
		return -1;
	}

	uint8_t *mem_ptr = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE,
				MAP_SHARED, fd, BASE_ADDRESS);

	if (mem_ptr == MAP_FAILED) {
		printf("Error mapping memory\n");
		return -1;
	}

	close(fd);

	uint32_t *const segment_register =
		(uint32_t *)(mem_ptr + SEGMENT_OFFSET);

	uint8_t *const button_register = mem_ptr + BUTTON_OFFSET;

	uint8_t *const led_register = mem_ptr + LEDS_OFFSET;

	int8_t counter = 0;
	set_7_segment(segment_register, counter);

	while (1) {
		volatile int button_state = get_key_state(button_register);

		if (button_state & KEY_0_MASK) {
			if (counter < MAX)
				counter++;
		} else if (button_state & KEY_1_MASK) {
			if (counter > MIN)
				counter--;
		} else if (button_state & KEY_2_MASK) {
			counter = 0;
		} else if (button_state & KEY_3_MASK) {
			break;
		}

		if (button_state != 0) {
			printf("Counter: %d\n", counter);
			set_7_segment(segment_register, counter);
			set_leds(led_register, counter);
		}
	}

	clear_7_segment(segment_register);
	munmap(mem_ptr, getpagesize());

	return 0;
}