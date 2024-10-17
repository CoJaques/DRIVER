/*      
Title  : Lab 2 - Exercise 1
Author : Colin Jaques
Date   : 2023-10-10
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define BASE_ADDRESS	0xFF200000
#define SWITCHES_OFFSET 0x40
#define LEDS_OFFSET	0x0
#define SEGMENT1_OFFSET 0x20
#define SEGMENT2_OFFSET 0x30
#define BUTTON_OFFSET	0x50
#define OFFSET		0x8
#define MAX		99999999
#define SWITCHES_MASK	0x3FF

// Enumeration for button masks to improve readability
typedef enum {
	KEY_0 = 1 << 0,
	KEY_1 = 1 << 1,
	KEY_2 = 1 << 2,
	KEY_3 = 1 << 3
} Button;

// Lookup table for 7-segment display numbers
static const uint32_t number_representation_7segment[] = {
	0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

// Set the 7-segment display based on the value to print (max 999999)
void set_7_segment(uint32_t *segment1_register, uint32_t *segment2_register,
		   unsigned value_to_print)
{
	if (value_to_print > 999999) {
		value_to_print = 999999; // Cap value at 999999
	}

	// Extract digits from the value
	uint8_t unit = value_to_print % 10;
	uint8_t ten = (value_to_print / 10) % 10;
	uint8_t hundred = (value_to_print / 100) % 10;
	uint8_t thousand = (value_to_print / 1000) % 10;
	uint8_t ten_thousand = (value_to_print / 10000) % 10;
	uint8_t hundred_thousand = (value_to_print / 100000) % 10;

	// Set the values for the 7-segment registers
	uint32_t segment1_value =
		number_representation_7segment[unit] |
		(number_representation_7segment[ten] << OFFSET) |
		(number_representation_7segment[hundred] << (OFFSET * 2)) |
		(number_representation_7segment[thousand] << (OFFSET * 3));

	uint32_t segment2_value =
		number_representation_7segment[ten_thousand] |
		(number_representation_7segment[hundred_thousand] << OFFSET);

	*segment1_register = segment1_value;
	*segment2_register = segment2_value;
}

// Set LEDs based on value
void set_leds(uint32_t *base_address, unsigned value_to_print)
{
	*base_address = value_to_print > MAX;
}

// Clear all outputs (7-segment and LEDs)
void clear_output(uint32_t *segment1, uint32_t *segment2, uint32_t *led)
{
	*segment1 = 0x0;
	*segment2 = 0x0;
	*led = 0x0;
}

// Return button state with edge detection
int get_key_state(uint8_t *addr)
{
	static uint8_t last_state = 0;
	uint8_t current_state = *addr;

	int key_pressed = (last_state ^ current_state) & current_state;
	last_state = current_state;
	return key_pressed;
}

// Read and mask switch states
uint32_t read_switches(uint32_t *switches)
{
	uint32_t value = *switches & SWITCHES_MASK;
	return value;
}

int main(void)
{
	// Open /dev/mem to access physical memory
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

	uint32_t *const segment1_register =
		(uint32_t *)(mem_ptr + SEGMENT1_OFFSET);

	uint32_t *const segment2_register =
		(uint32_t *)(mem_ptr + SEGMENT2_OFFSET);

	uint8_t *const button_register = mem_ptr + BUTTON_OFFSET;
	uint32_t *const led_register = (uint32_t *)(mem_ptr + LEDS_OFFSET);

	uint32_t *const switch_register =
		(uint32_t *)(mem_ptr + SWITCHES_OFFSET);

	unsigned counter = 0;

	set_7_segment(segment1_register, segment2_register, counter);
	set_leds(led_register, counter);

	while (1) {
		volatile int button_state = get_key_state(button_register);

		if (button_state & KEY_0) {
			if (counter < MAX)
				counter += read_switches(switch_register);
		} else if (button_state & KEY_1) {
			counter = 0;
		} else if (button_state & KEY_3) {
			break; // Exit loop on KEY_3 press
		}

		// Update display and LEDs only if a button was pressed
		if (button_state != 0) {
			printf("Counter: %d\n", counter);
			set_7_segment(segment1_register, segment2_register,
				      counter);
			set_leds(led_register, counter);
		}
	}

	// Unmap memory before exiting
	clear_output(segment1_register, segment2_register, led_register);
	munmap(mem_ptr, getpagesize());

	return EXIT_SUCCESS;
}
