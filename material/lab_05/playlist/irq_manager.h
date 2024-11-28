#ifndef IRQ_MANAGER_H
#define IRQ_MANAGER_H

#include "linux/irqreturn.h"
#include "driver_types.h"

#define SEGMENT1_OFFSET	      0x20
#define LED_OFFSET	      0x00
#define BUTTON_OFFSET	      0x50
#define BUTTON_EDGE_OFFSET    0x5C
#define BUTTON_INTERRUPT_MASK 0x58

// Button Masks
typedef enum {
	KEY_0 = 1 << 0,
	KEY_1 = 1 << 1,
	KEY_2 = 1 << 2,
	KEY_3 = 1 << 3
} Button;

irqreturn_t irq_handler(int irq, void *dev_id);
int setup_hw_irq(struct priv *priv, struct platform_device *pdev,
		 const char *name);
void cleanup_irq(struct priv *priv);

#endif // IRQ_MANAGER_H
