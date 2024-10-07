#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define BASE_ADDRESS 0xFF200000
#define SEGMENT_OFFSET 0x20
#define SEGMENT_SIZE 0x8
#define BUTTON_OFFSET 0x50
#define KEY_0_MASK (1 << 0)
#define KEY_1_MASK (1 << 1)
#define KEY_2_MASK (1 << 2)
#define KEY_3_MASK (1 << 3)

static const uint8_t number_representation_7segment[] = {
	0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

void set_7_segment(uint8_t *base_address, unsigned value_to_print, unsigned segment_number)
{
	int value = number_representation_7segment[value];
	uint8_t adress = base_address + (OFFSET * segment_number);
	*address = value;
}

void clear_7_segment(uint8_t *addr)
{
	memset((void *)addr, 0, 2);
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
	int fd = open("/dev/uio0", O_RDWR);
	if (fd < 0) {
		printf("Error opening /dev/uio0\n");
		return -1;
	}

	uint8_t *uio_ptr = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE,
				MAP_SHARED, fd, BASE_ADDRESS);

	if (uio_ptr == MAP_FAILED) {
		printf("Error mapping memory\n");
		return -1;
	}

	close(fd);

	uint8_t *const segment_register = uio_ptr + SEGMENT_OFFSET;
	uint8_t *const button_register = uio_ptr + BUTTON_OFFSET;

	clear_7_segment(segment_register);

	int8_t counter = 0;

	while (1) {
		volatile int button_state = get_key_state(button_register);

		if (button_state & KEY_0_MASK) {
			counter++;
		} else if (button_state & KEY_1_MASK) {
			counter--;
		} else if (button_state & KEY_2_MASK) {
			counter = 0;
		} else if (button_state & KEY_3_MASK) {
			break;
		}

		unsigned unit = counter % 10;
		unsigned diz = counter / 10;

		if (button_state != 0) {
			printf("Counter: %d\n", counter);
			clear_7_segment(segment_register);
			set_7_segment(segment_register, unit, 0);
			set_7_segment(segment_register, diz, 1);
		}
	}

	return 0;
}