#include "io_manager.h"
#include "driver_types.h"

// Lookup table for 7-segment display numbers
static const uint32_t number_representation_7segment[] = {
	0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

static void set_7_segment(uint32_t value, struct io_registers *io)
{
	uint32_t segment1_value = 0;

	if (value > MAX_VALUE_SEGMENT) {
		value = MAX_VALUE_SEGMENT;
	}

	for (unsigned i = 0; i < 4; i++) {
		segment1_value |= number_representation_7segment[value % 10]
				  << (SEGMENT_VOID_OFFSET * i);
		value /= 10;
	}

	iowrite32(segment1_value, io->segment1);
}

void set_time_segment(uint32_t seconds, struct io_registers *io)
{
	uint32_t minutes = seconds / 60;
	seconds = seconds % 60;

	set_7_segment(minutes * 100 + seconds, io);
}

void set_running_led(bool value, struct io_registers *io)
{
	uint16_t led_value;

	led_value = ioread16(io->led);
	led_value = value ? (led_value | (1 << RUNNING_LED_OFFSET)) :
			    (led_value & ~(1 << RUNNING_LED_OFFSET));
	iowrite16(led_value, io->led);
}

void set_counting_led(uint8_t value, struct io_registers *io)
{
	uint16_t led_value;

	led_value = ioread16(io->led);
	led_value &= ~COUNTING_LED_MASK;
	led_value |= value & COUNTING_LED_MASK;
	iowrite16(led_value, io->led);
}

void map_io(struct io_registers *io, void __iomem *base)
{
	io->segment1 = base + SEGMENT1_OFFSET;
	io->led = base + LED_OFFSET;
	io->button = base + BUTTON_OFFSET;
	io->button_edge = base + BUTTON_EDGE_OFFSET;
	io->button_interrupt_mask = base + BUTTON_INTERRUPT_MASK;
}
