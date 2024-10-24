/*      
Title  : Lab 2 - Exercise 4
Author : Colin Jaques
Date   : 2023-10-10
*/

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/uio.h>
#include "device.h"

#define SWITCHES_OFFSET	      0x40
#define LEDS_OFFSET	      0x0
#define SEGMENT1_OFFSET	      0x20
#define SEGMENT2_OFFSET	      0x30
#define BUTTON_OFFSET	      0x50
#define OFFSET		      0x8
#define MAX		      999999
#define MIN		      (-999999)
#define SWITCHES_MASK_VALUE   0x1FF
#define SWITCHES_MASK_SIGN    0x200
#define SIGN_LEDS	      0x9

#define UIO_NAME	      "/sys/class/uio/uio0/name"
#define UIO_MEM_SIZE	      "/sys/class/uio/uio0/maps/map0/size"
#define DEVICE		      "/dev/uio0"
#define EXPECTED_NAME	      "drv2024"

#define INTERRUPT_MASK_OFFSET 0x08
#define INTERRUPT_EDGE_OFFSET 0x0C
#define INTERRUPT_CTRL_BUTTON INTERRUPT_MASK_OFFSET + BUTTON_OFFSET
#define INTERRUPT_EDGE_BUTTON INTERRUPT_EDGE_OFFSET + BUTTON_OFFSET

// Enumeration for button masks to improve readability
typedef enum {
	KEY_0 = 1 << 0,
	KEY_1 = 1 << 1,
	KEY_2 = 1 << 2,
	KEY_3 = 1 << 3
} Button;

static int running = 1;

void handle_sigint(int sig)
{
	printf("\nCaught signal %d (SIGINT), exiting cleanly...\n", sig);
	running = 0;
}

void init_signal(struct sigaction *sig)
{
	sig->sa_handler =
		handle_sigint; // Définir la fonction de gestion du signal
	sigemptyset(
		&sig->sa_mask); // Aucune autre signal n'est bloqué pendant l'exécution
	sig->sa_flags = 0; // Pas d'options spéciales

	// Installer le gestionnaire de SIGINT
	if (sigaction(SIGINT, sig, NULL) == -1) {
		perror("Error: cannot handle SIGINT"); // Gérer l'erreur si sigaction échoue
		exit(EXIT_FAILURE);
	}
}

// Lookup table for 7-segment display numbers
static const uint32_t number_representation_7segment[] = {
	0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

// Set the 7-segment display based on the value to print (max 999999)
void set_7_segment(uint32_t *segment1_register, uint32_t *segment2_register,
		   unsigned value_to_print)
{
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

void set_leds(uint32_t *base_address, uint8_t on)
{
	*base_address = on ? 1 << SIGN_LEDS : 0;
}

// Clear all outputs (7-segment and LEDs)
void clear_output(uint32_t *segment1, uint32_t *segment2, uint32_t *led)
{
	*segment1 = 0x0;
	*segment2 = 0x0;
	*led = 0x0;
}

int16_t read_switches(uint32_t *switches)
{
	// Read the switch state (9 bits value + 1 sign bit)
	int16_t data = *switches;

	// Extract the value (bits [8:0])
	int16_t value = data & SWITCHES_MASK_VALUE;

	// If the sign bit is set, return the negative value using two's complementi
	return (data & SWITCHES_MASK_SIGN) ? (~value + 1) : value;
}

int process_counter(int counter, int16_t button_value, uint8_t button_state)
{
	switch (button_state) {
	case KEY_0:
		printf("%d + %d\n", counter, button_value);
		counter += button_value;
		break;
	case KEY_1:
		printf("%d - %d\n", counter, button_value);
		counter -= button_value;
		break;
	case KEY_2:
		printf("%d * %d\n", counter, button_value);
		counter *= button_value;
		break;
	case KEY_3:
		if (button_value != 0) {
			printf("%d / %d\n", counter, button_value);
			counter /= button_value;
		} else {
			printf("it's impossible to divide by 0. \n");
		}
		break;
	default:
		break;
	}

	if (counter > MAX) {
		printf("counter > max : %d %d \n", counter, MAX);
		counter = MAX;
	} else if (counter < MIN) {
		printf("counter < min : %d %d \n", counter, MIN);
		counter = MIN;
	}

	printf("Counter: %d\n", counter);
	return counter;
}

int main(void)
{
	struct sigaction sa;
	init_signal(&sa);

	if (check_device_name(UIO_NAME, EXPECTED_NAME) != 0) {
		return EXIT_FAILURE;
	}

	if (check_memory_size(UIO_MEM_SIZE, getpagesize()) != 0) {
		return EXIT_FAILURE;
	}

	// Open /dev/mem to access physical memory
	int fd = open("/dev/uio0", O_RDWR | O_SYNC);
	if (fd < 0) {
		perror("Error opening /dev/uio0");
		return EXIT_FAILURE;
	}

	// Map the physical memory to virtual memory
	uint8_t *mem_ptr = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE,
				MAP_SHARED, fd, 0);
	if (mem_ptr == MAP_FAILED) {
		perror("Error mapping memory");
		close(fd);
		return EXIT_FAILURE;
	}

	uint32_t *const segment1_register =
		(uint32_t *)(mem_ptr + SEGMENT1_OFFSET);

	uint32_t *const segment2_register =
		(uint32_t *)(mem_ptr + SEGMENT2_OFFSET);

	uint32_t *const led_register = (uint32_t *)(mem_ptr + LEDS_OFFSET);

	uint32_t *const switch_register =
		(uint32_t *)(mem_ptr + SWITCHES_OFFSET);

	set_7_segment(segment1_register, segment2_register, 0);
	set_leds(led_register, 0);

	// enbable interupt and clear it
	*(mem_ptr + INTERRUPT_CTRL_BUTTON) = 0xF;
	*(mem_ptr + INTERRUPT_EDGE_BUTTON) = 0xF;

	int32_t counter = 0;
	while (running) {
		volatile uint8_t button_state = 0;
		uint32_t info = 1;

		ssize_t nb = write(fd, &info, sizeof(info));
		if (nb != (ssize_t)sizeof(info)) {
			perror("write interupt mask error");
			clear_output(segment1_register, segment2_register,
				     led_register);
			close(fd);
			munmap(mem_ptr, getpagesize());
			break;
		}

		nb = read(fd, &info, sizeof(info));
		if (nb == (ssize_t)sizeof(info)) {
			button_state = *(mem_ptr + INTERRUPT_EDGE_BUTTON);
			int16_t value = read_switches(switch_register);
			counter = process_counter(counter, value, button_state);
			set_7_segment(segment1_register, segment2_register,
				      abs(counter));
			set_leds(led_register, counter < 0);

			// clear interupt
			*(mem_ptr + INTERRUPT_EDGE_BUTTON) = 0xF;
		}
	}
	clear_output(segment1_register, segment2_register, led_register);
	munmap(mem_ptr, getpagesize());
	close(fd);
	return EXIT_SUCCESS;
}
