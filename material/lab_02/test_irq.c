#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define SWITCHES_OFFSET	      0x40
#define LEDS_OFFSET	      0x0
#define SEGMENT1_OFFSET	      0x20
#define SEGMENT2_OFFSET	      0x30
#define BUTTON_OFFSET	      0x50
#define OFFSET		      0x8
#define MAX		      99999999
#define SWITCHES_MASK	      0xF

#define INTERRUPT_MASK_OFFSET 0x58 // Offset for Interruptmask register
#define EDGE_CAPTURE_OFFSET   0x5C // Offset for Edgecapture register
#define BUTTON_OFFSET	      0x50 // Offset for button data register

int main(void)
{
	printf("start program");
	int fd = open("/dev/uio0", O_RDWR);
	uint8_t *mem_ptr = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE,
				MAP_SHARED, fd, 0);

	*(mem_ptr + INTERRUPT_MASK_OFFSET) =
		0xF; // Enable interrupts on all 4 buttons (bits 3-0)

	*(mem_ptr + EDGE_CAPTURE_OFFSET) = 0xF;

	while (1) {
		uint32_t info = 1; /* unmask */

		ssize_t nb = write(fd, &info, sizeof(info));

		nb = read(fd, &info, sizeof(info));

		if (nb == (ssize_t)sizeof(info)) {
			// Read the button press (KEY3-0)
			uint8_t buttons = *(mem_ptr + EDGE_CAPTURE_OFFSET);
			printf("Buttons state: 0x%x\n", buttons);

			// Clear the interrupt by writing to the Edgecapture register
			*(mem_ptr + EDGE_CAPTURE_OFFSET) =
				0xF; // Clear interrupts on all 4 buttons
		}
	}

	close(fd);
	exit(EXIT_SUCCESS);
}
