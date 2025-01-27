// COLIN JAQUES
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/delay.h>

#include "driver_types.h"
#include "io_manager.h"
#include "match_manager.h"

#define DEVICE_NAME "tennis"

#define CLEANUP_ON_ERROR(action, label, dev, message) \
	do {                                          \
		if ((ret = action) < 0) {             \
			dev_err(dev, message);        \
			goto label;                   \
		}                                     \
	} while (0)

static int tennis_probe(struct platform_device *pdev)
{
	struct priv *priv;
	struct resource *mem_info;
	void __iomem *base_address;
	int ret;

	dev_info(&pdev->dev, "Probing tennis driver\n");

	// Allocate private data structure
	priv = devm_kzalloc(&pdev->dev, sizeof(struct priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->io.dev = &pdev->dev;

	// Get and map memory resource
	mem_info = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_info) {
		dev_err(&pdev->dev, "Failed to get memory resource\n");
		ret = -EINVAL;
		goto FREE_PRIV;
	}

	base_address = devm_ioremap_resource(&pdev->dev, mem_info);
	if (IS_ERR(base_address)) {
		dev_err(&pdev->dev, "Failed to remap memory\n");
		ret = PTR_ERR(base_address);
		goto FREE_PRIV;
	}

	// Map IO registers
	map_io(&priv->io, base_address);

	// Create workqueue
	priv->announcement_wq =
		create_singlethread_workqueue("tennis_announcements");
	if (!priv->announcement_wq) {
		dev_err(&pdev->dev, "Failed to create workqueue\n");
		ret = -ENOMEM;
		goto FREE_PRIV;
	}

	// Initialize display state
	INIT_LIST_HEAD(&priv->display.pending_announcements);
	priv->display.mode = DISPLAY_NORMAL;
	priv->display.summary_alternate = false;

	// Initialize match data
	priv->party.match_in_progress = false;
	priv->party.format = 2; // Default: best of 3 sets
	priv->party.advantage_player = PLAYER_NONE;

	// Initialize timer
	hrtimer_init(&priv->summary_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	priv->summary_timer.function = summary_display_timer;

	// Setup match IRQ
	CLEANUP_ON_ERROR(setup_match_irq(priv, pdev), DESTROY_WORKQUEUE,
			 &pdev->dev, "Failed to setup match IRQ\n");

	dev_info(&pdev->dev, "Tennis driver initialized\n");
	return 0;

DESTROY_WORKQUEUE:
	destroy_workqueue(priv->announcement_wq);
FREE_PRIV:
	// No need to free priv as it's managed by devm_kzalloc
	return ret;
}

static int tennis_remove(struct platform_device *pdev)
{
	struct priv *priv = platform_get_drvdata(pdev);

	if (!priv) {
		pr_err("Failed to get private data\n");
		return -EINVAL;
	}

	dev_info(priv->io.dev, "Removing tennis driver\n");

	// Destroy workqueue
	if (priv->announcement_wq) {
		flush_workqueue(priv->announcement_wq);
		destroy_workqueue(priv->announcement_wq);
	}

	stop_summary_display(priv);

	// Clear displays
	iowrite32(0, priv->io.segment1);
	iowrite32(0, priv->io.segment2);
	iowrite32(0, priv->io.led);

	return 0;
}

static const struct of_device_id tennis_driver_id[] = { { .compatible =
								  "drv2024" },
							{ /* sentinel */ } };
MODULE_DEVICE_TABLE(of, tennis_driver_id);

static struct platform_driver tennis_driver = {
    .driver = {
        .name = "tennis",
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(tennis_driver_id),
    },
    .probe = tennis_probe,
    .remove = tennis_remove,
};

module_platform_driver(tennis_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Colin Jaques");
MODULE_DESCRIPTION("Tennis match driver");
