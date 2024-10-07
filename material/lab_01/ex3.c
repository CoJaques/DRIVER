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
#define MIN	       0
#define MAX	       99

// Enumeration for button masks for better readability
typedef enum {
	KEY_0 = 1 << 0,
	KEY_1 = 1 << 1,
	KEY_2 = 1 << 2,
	KEY_3 = 1 << 3
} Button;

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

	printf("Value: %d Setting 7-segment value to 0x%X at address %p\n",
	       value_to_print, value, base_address);
}

void set_leds(uint32_t *base_address, unsigned value_to_print)
{
	// Turn on LEDs 1 to 9 depending on the unit of the counter, turn off if 0.
	*base_address = value_to_print > 0 ? (1 << (value_to_print - 1)) : 0;
}

void clear_output(uint32_t *segment, uint32_t *led)
{
	*segment = 0x0;
	*led = 0x0;
}

// Return the state of the button if the value changed since the last call (edge detection)
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
	// Open /dev/mem for accessing physical memory
	int fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) {
		perror("Error opening /dev/mem");
		return EXIT_FAILURE;
	}

	// Map the physical memory to virtual memory
	uint8_t *mem_ptr = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE,
				MAP_SHARED, fd, BASE_ADDRESS);
	if (mem_ptr == MAP_FAILED) {
		perror("Error mapping memory");
		close(fd);
		return EXIT_FAILURE;
	}

	close(fd); // Close the file descriptor after mapping

	uint32_t *const segment_register =
		(uint32_t *)(mem_ptr + SEGMENT_OFFSET);
	uint8_t *const button_register = mem_ptr + BUTTON_OFFSET;
	uint32_t *const led_register = (uint32_t *)(mem_ptr + LEDS_OFFSET);

	int8_t counter = 0;

	set_7_segment(segment_register, counter);
	set_leds(led_register, counter);

	while (1) {
		volatile int button_state = get_key_state(button_register);

		if (button_state & KEY_0) {
			if (counter < MAX)
				counter++;
		} else if (button_state & KEY_1) {
			if (counter > MIN)
				counter--;
		} else if (button_state & KEY_2) {
			counter = 0;
		} else if (button_state & KEY_3) {
			break; // Exit the loop on KEY_3 press
		}

		// Update display and LEDs only if a button was pressed
		if (button_state != 0) {
			printf("Counter: %d\n", counter);
			set_7_segment(segment_register, counter);
			set_leds(led_register, counter);
		}
	}

	// Unmap the memory before exiting
	clear_output(segment_register, led_register);
	munmap(mem_ptr, getpagesize());

	return EXIT_SUCCESS;
}
