#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/of.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define BUTTON_OFFSET	      0x50
#define BUTTON_EDGE_OFFSET    0x5C
#define BUTTON_INTERRUPT_MASK 0x58
#define SWITCH_OFFSET	      0x40
#define SWITCHES_MASK	      0x3FF
#define SEGMENT1_OFFSET	      0x20
#define SEGMENT2_OFFSET	      0x30
#define INTERRUPT_MASK_OFFSET 0x58
#define EDGE_CAPTURE_OFFSET   0x5C
#define BASE_ADDRESS	      0xFF200000
#define SEGMENT_VOID_OFFSET   0x08

// Enumeration for button masks to improve readability
typedef enum {
	KEY_0 = 1 << 0,
	KEY_1 = 1 << 1,
	KEY_2 = 1 << 2,
	KEY_3 = 1 << 3
} Button;

struct priv {
	void __iomem *switches;
	void __iomem *segment1;
	void __iomem *segment2;
	void __iomem *button;
	void __iomem *button_edge;
	void __iomem *button_interrupt_mask;

	uint32_t counter;
	int irq_num;
	struct device *dev;
};
// Lookup table for 7-segment display numbers
static const uint32_t number_representation_7segment[] = {
	0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

// Set the 7-segment display based on the value to print (max 999999)
static void set_7_segment(uint32_t value, struct priv *priv)
{
	if (value > 999999) {
		value = 999999; // cap value to 999999
	}

	// Extract digits from the value
	uint8_t unit = value % 10;
	uint8_t ten = (value / 10) % 10;
	uint8_t hundred = (value / 100) % 10;
	uint8_t thousand = (value / 1000) % 10;
	uint8_t ten_thousand = (value / 10000) % 10;
	uint8_t hundred_thousand = (value / 100000) % 10;

	// Set the values for the 7-segment registers
	uint32_t segment1_value =
		number_representation_7segment[unit] |
		(number_representation_7segment[ten] << SEGMENT_VOID_OFFSET) |
		(number_representation_7segment[hundred]
		 << (SEGMENT_VOID_OFFSET * 2)) |
		(number_representation_7segment[thousand]
		 << (SEGMENT_VOID_OFFSET * 3));

	uint32_t segment2_value =
		number_representation_7segment[ten_thousand] |
		(number_representation_7segment[hundred_thousand]
		 << SEGMENT_VOID_OFFSET);

	iowrite32(segment1_value, priv->segment1);
	iowrite32(segment2_value, priv->segment2);
}

static irqreturn_t irq_handler(int irq, void *dev_id)
{
	struct priv *priv = (struct priv *)dev_id;
	uint8_t last_pressed_button = ioread8(priv->button_edge);

	switch (last_pressed_button) {
	case KEY_0:
		priv->counter = ioread16(priv->switches) & SWITCHES_MASK;
		break;
	case KEY_1:
		priv->counter++;
		break;
	}

	set_7_segment(priv->counter, priv);

	iowrite8(0x0F, priv->button_edge);

	return IRQ_HANDLED;
}

static int switch_copy_probe(struct platform_device *pdev)
{
	void __iomem *base_address;
	struct priv *priv;
	struct resource *mem_info;
	int ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(struct priv), GFP_KERNEL);
	if (unlikely(!priv))
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);

	priv->dev = &pdev->dev;

	mem_info = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (unlikely(!mem_info)) {
		dev_err(priv->dev, "Failed to get memory resource\n");
		return -EINVAL;
	}

	base_address = devm_ioremap_resource(priv->dev, mem_info);
	if (IS_ERR(base_address)) {
		dev_err(priv->dev, "Failed to map memory resource\n");
		return PTR_ERR(base_address);
	}

	dev_info(priv->dev, "Registering interrupt handler\n");
	priv->irq_num = platform_get_irq(pdev, 0);
	if (priv->irq_num < 0) {
		dev_err(priv->dev, "Failed to get IRQ number\n");
		return -EINVAL;
	}

	ret = devm_request_irq(&pdev->dev, priv->irq_num, irq_handler, 0,
			       "switch_copy", priv);
	if (ret < 0) {
		dev_err(priv->dev, "Failed to request IRQ\n");
		return ret;
	}

	priv->switches = base_address + SWITCH_OFFSET;
	priv->segment1 = base_address + SEGMENT1_OFFSET;
	priv->segment2 = base_address + SEGMENT2_OFFSET;
	priv->button = base_address + BUTTON_OFFSET;
	priv->button_edge = base_address + BUTTON_EDGE_OFFSET;
	priv->button_interrupt_mask = base_address + BUTTON_INTERRUPT_MASK;

	// enable interrupts on hw
	iowrite8(0xF, priv->button_interrupt_mask);

	// rearm interrupts
	iowrite8(0x0F, priv->button_edge);

	return 0;
}

static int switch_copy_remove(struct platform_device *pdev)
{
	struct priv *priv = platform_get_drvdata(pdev);
	if (!priv) {
		pr_err("Failed to get private data\n");
		return -EINVAL;
	}

	dev_info(priv->dev, "Removing interrupt handler\n");

	// disable interrupts on hw
	iowrite8(0x0, priv->button_interrupt_mask);

	// clear 7seg
	iowrite32(0, priv->segment1);
	iowrite32(0, priv->segment2);

	kfree(priv);
	return 0;
}

static const struct of_device_id switch_copy_driver_id[] = {
	{ .compatible = "drv2024" },
	{ /* END */ },
};

MODULE_DEVICE_TABLE(of, switch_copy_driver_id);

static struct platform_driver switch_copy_driver = {
	.driver = {
		.name = "drv-lab4",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(switch_copy_driver_id),
	},
	.probe = switch_copy_probe,
	.remove = switch_copy_remove,
};

module_platform_driver(switch_copy_driver);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("REDS");
MODULE_DESCRIPTION("Introduction to the interrupt and platform drivers");
