// COLIN JAQUES

#include "linux/hrtimer.h"
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
#include <linux/kthread.h>

#define BUTTON_OFFSET	      0x50
#define BUTTON_EDGE_OFFSET    0x5C
#define BUTTON_INTERRUPT_MASK 0x58
#define SEGMENT1_OFFSET	      0x20
#define LED_OFFSET	      0x3A
#define INTERRUPT_MASK_OFFSET 0x58
#define EDGE_CAPTURE_OFFSET   0x5C
#define SEGMENT_VOID_OFFSET   0x08
#define MAX_VALUE_SEGMENT     9999

// Enumeration for button masks to improve readability
typedef enum {
	KEY_0 = 1 << 0,
	KEY_1 = 1 << 1,
	KEY_2 = 1 << 2,
	KEY_3 = 1 << 3
} Button;

struct io_registers {
	void __iomem *segment1;
	void __iomem *led;
	void __iomem *button;
	void __iomem *button_edge;
	void __iomem *button_interrupt_mask;
};

struct time_management {
	uint32_t current_time;
	struct hrtimer music_timer;
	struct task_struct *display_thread;
};

struct priv {
	struct io_registers io;
	struct time_management time;
	struct device *dev;
	bool is_playing;
};

// Lookup table for 7-segment display numbers
static const uint32_t number_representation_7segment[] = {
	0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

// Set the 7-segment display based on the value to print
static void set_7_segment(uint32_t value, struct priv *priv)
{
	if (value > MAX_VALUE_SEGMENT) {
		value = MAX_VALUE_SEGMENT;
	}

	// Extract digits from the value
	uint8_t unit = value % 10;
	uint8_t ten = (value / 10) % 10;
	uint8_t hundred = (value / 100) % 10;
	uint8_t thousand = (value / 1000) % 10;

	// Set the values for the 7-segment registers
	uint32_t segment1_value =
		number_representation_7segment[unit] |
		(number_representation_7segment[ten] << SEGMENT_VOID_OFFSET) |
		(number_representation_7segment[hundred]
		 << (SEGMENT_VOID_OFFSET * 2)) |
		(number_representation_7segment[thousand]
		 << (SEGMENT_VOID_OFFSET * 3));

	iowrite32(segment1_value, priv->io.segment1);
}

static void set_time_segment(uint32_t seconds, struct priv *priv)
{
	uint32_t minutes = seconds / 60;
	seconds = seconds % 60;
	set_7_segment(minutes * 100 + seconds, priv);
}

static irqreturn_t irq_handler(int irq, void *dev_id)
{
	struct priv *priv = (struct priv *)dev_id;
	uint8_t last_pressed_button = ioread8(priv->io.button_edge);

	// TODO CJS -> Add behaviour for key
	switch (last_pressed_button) {
	case KEY_0:
		priv->is_playing = !priv->is_playing; // Alterne Play/Pause
		if (priv->is_playing) {
			hrtimer_start(&priv->time.music_timer, ktime_set(1, 0),
				      HRTIMER_MODE_REL);
		} else {
			hrtimer_cancel(&priv->time.music_timer);
		}
		// add play/pause
		break;
	case KEY_1:
		priv->time.current_time = 0;
		set_7_segment(0, priv);
		break;
		// add reset
		break;
	case KEY_2:
		priv->time.current_time = 600;
		// add next
		break;
	case KEY_3:
		// add exit
		break;
	}

	iowrite8(0x0F, priv->io.button_edge);

	return IRQ_HANDLED;
}

static int display_thread_func(void *data)
{
	struct priv *priv = (struct priv *)data;

	while (!kthread_should_stop()) {
		if (priv->is_playing) {
			set_time_segment(priv->time.current_time, priv);
			priv->time.current_time++;
		}
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}
	return 0;
}

static enum hrtimer_restart timer_callback(struct hrtimer *timer)
{
	struct priv *priv = container_of(timer, struct priv, time.music_timer);

	if (priv->is_playing) {
		wake_up_process(priv->time.display_thread);
		hrtimer_forward_now(timer, ktime_set(1, 0));
		return HRTIMER_RESTART;
	}
	return HRTIMER_NORESTART;
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
	int irq_num = platform_get_irq(pdev, 0);
	if (irq_num < 0) {
		dev_err(priv->dev, "Failed to get IRQ number\n");
		return -EINVAL;
	}

	ret = devm_request_irq(&pdev->dev, irq_num, irq_handler, 0, "playlist",
			       priv);
	if (ret < 0) {
		dev_err(priv->dev, "Failed to request IRQ\n");
		return ret;
	}

	priv->io.segment1 = base_address + SEGMENT1_OFFSET;
	priv->io.led = base_address + LED_OFFSET;
	priv->io.button = base_address + BUTTON_OFFSET;
	priv->io.button_edge = base_address + BUTTON_EDGE_OFFSET;
	priv->io.button_interrupt_mask = base_address + BUTTON_INTERRUPT_MASK;

	// enable interrupts on hw
	iowrite8(0xF, priv->io.button_interrupt_mask);

	// rearm interrupts
	iowrite8(0x0F, priv->io.button_edge);
	set_7_segment(0, priv);

	hrtimer_init(&priv->time.music_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	priv->time.music_timer.function = timer_callback;

	priv->time.current_time = 0;
	priv->time.display_thread =
		kthread_create(display_thread_func, priv, "display_thread");
	if (IS_ERR(priv->time.display_thread)) {
		dev_err(priv->dev, "Failed to create display thread\n");
		return PTR_ERR(priv->time.display_thread);
	}
	wake_up_process(priv->time.display_thread);

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
	iowrite8(0x0, priv->io.button_interrupt_mask);

	// clear 7seg
	iowrite32(0, priv->io.segment1);

	hrtimer_cancel(&priv->time.music_timer);
	kthread_stop(priv->time.display_thread);

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
		.name = "drivify",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(switch_copy_driver_id),
	},
	.probe = switch_copy_probe,
	.remove = switch_copy_remove,
};

module_platform_driver(switch_copy_driver);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("COJAK");
MODULE_DESCRIPTION("Bleeding edge playlist driver (really ?)");
