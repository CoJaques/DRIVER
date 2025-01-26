#ifndef MATCH_MANAGER_H
#define MATCH_MANAGER_H

#include <linux/platform_device.h>
#include "driver_types.h"

int setup_match_irq(struct priv *priv, struct platform_device *pdev);

#endif // MATCH_MANAGER_H
