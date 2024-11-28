#ifndef IO_MANAGER_H
#define IO_MANAGER_H

#include <linux/types.h>
#include "driver_types.h"

#define SEGMENT_VOID_OFFSET 8
#define MAX_VALUE_SEGMENT   9999
#define RUNNING_LED_OFFSET  0x8

void set_7_segment(uint32_t value, struct priv *priv);
void set_time_segment(uint32_t seconds, struct priv *priv);
void set_running_led(bool value, struct priv *priv);

#endif // IO_MANAGER_H
