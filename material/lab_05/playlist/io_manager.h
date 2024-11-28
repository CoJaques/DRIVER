#ifndef IO_MANAGER_H
#define IO_MANAGER_H

#include <linux/types.h>
#include "driver_types.h"

#define SEGMENT_VOID_OFFSET   8
#define MAX_VALUE_SEGMENT     9999
#define RUNNING_LED_OFFSET    0x8
#define SEGMENT1_OFFSET	      0x20
#define LED_OFFSET	      0x00
#define BUTTON_OFFSET	      0x50
#define BUTTON_EDGE_OFFSET    0x5C
#define BUTTON_INTERRUPT_MASK 0x58

/*
* This function is used to display a time in seconds on the 4 7 segments display.
* The time is displayed in the format MMSS.
*/
void set_time_segment(uint32_t seconds, struct io_registers *io);

/*
* This function is used to set the running led on or off.
*/
void set_running_led(bool value, struct io_registers *io);

/*
* This function is used to map the io registers to tha base address of the device.
*/
void map_io(struct io_registers *io, void __iomem *base);

#endif // IO_MANAGER_H
