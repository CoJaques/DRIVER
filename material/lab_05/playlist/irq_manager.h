#ifndef IRQ_MANAGER_H
#define IRQ_MANAGER_H

#include "driver_types.h"

// Button Masks
typedef enum {
	KEY_0 = 1 << 0,
	KEY_1 = 1 << 1,
	KEY_2 = 1 << 2,
	KEY_3 = 1 << 3
} Button;

/*
* This function is used to setup the hardware interrupt.
*/
int setup_hw_irq(struct priv *priv, struct platform_device *pdev,
		 const char *name);

/*
* This function is used to cleanup the hardware interrupt.
*/
void cleanup_irq(struct priv *priv);

#endif // IRQ_MANAGER_H
