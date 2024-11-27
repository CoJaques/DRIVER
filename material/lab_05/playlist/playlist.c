// COLIN JAQUESpl

#include "linux/hrtimer.h"
#include "linux/printk.h"
#include "linux/spinlock.h"
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
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>
#include <linux/fs.h>
#include <linux/math.h>

#define BUTTON_OFFSET	      0x50
#define BUTTON_EDGE_OFFSET    0x5C
#define BUTTON_INTERRUPT_MASK 0x58
#define SEGMENT1_OFFSET	      0x20
#define LED_OFFSET	      0x00
#define INTERRUPT_MASK_OFFSET 0x58
#define EDGE_CAPTURE_OFFSET   0x5C
#define SEGMENT_VOID_OFFSET   0x08
#define MAX_VALUE_SEGMENT     9999
#define RUNNING_LED_NUMBER    0x08
#define RUNNING_LED_OFFSET    (1 << RUNNING_LED_NUMBER)

#define DEVICE_NAME	      "drivify"
#define MAJOR_NUM	      99
#define MAJMIN		      MKDEV(MAJOR_NUM, 0)
#define MAX_PLAYLIST_SIZE     16

// Enumeration for button masks to improve readability
typedef enum {
	KEY_0 = 1 << 0,
	KEY_1 = 1 << 1,
	KEY_2 = 1 << 2,
	KEY_3 = 1 << 3
} Button;

struct io_registers {
	struct device *dev;
	void __iomem *segment1;
	void __iomem *led;
	void __iomem *button;
	void __iomem *button_edge;
	void __iomem *button_interrupt_mask;

	// Here we use spinlock in place of mutex
	// because in some case it's necessary to be
	// used in interrupt context
	spinlock_t led_running_spinlock;
	spinlock_t segments_spinlock;
};

struct time_management {
	uint32_t current_time;
	struct hrtimer music_timer;
	struct task_struct *display_thread;
};

struct music_data {
	uint16_t duration;
	char title[25];
	char artist[25];
};

struct playlist_data {
	struct device *dev;
	struct cdev cdev;
	struct class *cl;
	struct kfifo *playlist;
	struct music_data *current_music;
	bool next_music_requested;
};

struct priv {
	struct io_registers io;
	struct time_management time;
	struct playlist_data playlist_data;
	bool is_playing;
};

// Lookup table for 7-segment display numbers
static const uint32_t number_representation_7segment[] = {
	0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

// Set the 7-segment display based on the value to print
static void set_7_segment(uint32_t value, struct priv *priv)
{
	uint32_t segment1_value = 0;

	if (value > MAX_VALUE_SEGMENT) {
		value = MAX_VALUE_SEGMENT;
	}

	// Set the values for the 7-segment registers
	// We use the modulo operator to get the unit, ten, hundred and thousand
	for (unsigned i = 0; i < 4; i++) {
		segment1_value |= number_representation_7segment[value % 10]
				  << (SEGMENT_VOID_OFFSET * i);

		value /= 10;
	}

	iowrite32(segment1_value, priv->io.segment1);
}

static void set_time_segment(uint32_t seconds, struct priv *priv)
{
	// We use spinlock to protect the access to the segment register
	unsigned long flags;
	spin_lock_irqsave(&priv->io.segments_spinlock, flags);

	uint32_t minutes = seconds / 60;
	seconds = seconds % 60;
	set_7_segment(minutes * 100 + seconds, priv);

	spin_unlock_irqrestore(&priv->io.segments_spinlock, flags);
}

static void set_running_led(bool value, struct priv *priv)
{
	// We use spinlock to protect the access to the led register
	unsigned long flags;
	spin_lock_irqsave(&priv->io.led_running_spinlock, flags);

	uint16_t led_value = ioread16(priv->io.led);
	led_value = value ? (led_value | RUNNING_LED_OFFSET) :
			    (led_value & ~RUNNING_LED_OFFSET);
	iowrite16(led_value, priv->io.led);

	spin_unlock_irqrestore(&priv->io.led_running_spinlock, flags);
}

static int instanciate_music_if_null(struct priv *priv)
{
	if (priv->playlist_data.current_music == NULL) {
		priv->playlist_data.current_music =
			kmalloc(sizeof(struct music_data), GFP_KERNEL);
		if (priv->playlist_data.current_music == NULL) {
			pr_err("Failed to allocate memory for music data\n");
			return -1;
		}
	}

	return 0;
}

static int get_next_music_from_queue(struct priv *priv)
{
	if (kfifo_out(priv->playlist_data.playlist,
		      priv->playlist_data.current_music,
		      sizeof(struct music_data)) != sizeof(struct music_data)) {
		pr_err("Failed to get music data from playlist\n");
		kfree(priv->playlist_data.current_music);
		priv->playlist_data.current_music = NULL;
		return -1;
	}

	return 0;
}

static int next_music(struct priv *priv)
{
	pr_info("Next music\n");

	if (instanciate_music_if_null(priv))
		return -ENOMEM;

	if (get_next_music_from_queue(priv))
		return -EINVAL;

	pr_info("Playing music: '%s' by '%s', duration: %u seconds\n",
		priv->playlist_data.current_music->title,
		priv->playlist_data.current_music->artist,
		priv->playlist_data.current_music->duration);

	priv->time.current_time = 0;

	return 0;
}

static bool shouldStartPlayMusic(struct priv *priv)
{
	return (!priv->is_playing &&
		(priv->playlist_data.current_music != NULL ||
		 !kfifo_is_empty(priv->playlist_data.playlist)));
}

static irqreturn_t irq_handler(int irq, void *dev_id)
{
	struct priv *priv = (struct priv *)dev_id;
	uint8_t last_pressed_button = ioread8(priv->io.button_edge);

	switch (last_pressed_button) {
	case KEY_0:
		if (shouldStartPlayMusic(priv)) {
			priv->is_playing = true;

			// reset if a new music was requested
			priv->playlist_data.next_music_requested = false;

			hrtimer_start(&priv->time.music_timer, ktime_set(1, 0),
				      HRTIMER_MODE_REL);
			set_running_led(true, priv);
		} else {
			priv->is_playing = false;
			hrtimer_cancel(&priv->time.music_timer);
			set_running_led(false, priv);
		}
		break;
	case KEY_1:
		priv->time.current_time = 0;
		set_time_segment(priv->time.current_time, priv);
		break;
	case KEY_2:
		priv->playlist_data.next_music_requested = true;
		break;
	}

	iowrite8(0x0F, priv->io.button_edge);

	return IRQ_HANDLED;
}

static bool should_switch_music(struct priv *priv)
{
	return !priv->playlist_data.current_music || // No music is playing
	       priv->time.current_time >=
		       priv->playlist_data.current_music
			       ->duration || // Current music finished
	       priv->playlist_data.next_music_requested; // Next music requested
}

static void playlist_cycle(struct priv *priv)
{
	if (should_switch_music(priv)) {
		// Attempt to load the next music, stop playing if none available
		if (kfifo_is_empty(priv->playlist_data.playlist) ||
		    next_music(priv) != 0) {
			kfree(priv->playlist_data.current_music);
			priv->playlist_data.current_music = NULL;
			priv->time.current_time = 0;
			priv->is_playing = false;
			set_running_led(false, priv);
		}

		// Reset next music request flag
		priv->playlist_data.next_music_requested = false;
	}

	// Update the time display
	set_time_segment(priv->time.current_time, priv);
	priv->time.current_time++;
}

static int display_thread_func(void *data)
{
	struct priv *priv = (struct priv *)data;

	while (!kthread_should_stop()) {
		if (priv->is_playing)
			playlist_cycle(priv);

		// Put the thread to sleep until the next iteration
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
	} else {
		set_running_led(false, priv);
		return HRTIMER_NORESTART;
	}
}

static int setup_hw_irq(struct priv *priv, struct platform_device *pdev)
{
	void __iomem *base_address;
	struct resource *mem_info;
	int ret;

	mem_info = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (unlikely(!mem_info)) {
		dev_err(priv->io.dev, "Failed to get memory resource\n");
		return -EINVAL;
	}

	base_address = devm_ioremap_resource(priv->io.dev, mem_info);
	if (IS_ERR(base_address)) {
		dev_err(priv->io.dev, "Failed to map memory resource\n");
		return PTR_ERR(base_address);
	}

	int irq_num = platform_get_irq(pdev, 0);
	if (irq_num < 0) {
		dev_err(priv->io.dev, "Failed to get IRQ number\n");
		return -EINVAL;
	}

	ret = devm_request_irq(&pdev->dev, irq_num, irq_handler, 0, "playlist",
			       priv);
	if (ret < 0) {
		dev_err(priv->io.dev, "Failed to request IRQ\n");
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
	return 0;
}

static int setup_timer_thread(struct priv *priv)
{
	hrtimer_init(&priv->time.music_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	priv->time.music_timer.function = timer_callback;

	priv->time.current_time = 0;
	priv->time.display_thread =
		kthread_create(display_thread_func, priv, "display_thread");
	if (IS_ERR(priv->time.display_thread)) {
		dev_err(priv->io.dev, "Failed to create display thread\n");
		return PTR_ERR(priv->time.display_thread);
	}
	wake_up_process(priv->time.display_thread);
	return 0;
}

static int playlist_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	// Set the permissions of the device file
	add_uevent_var(env, "DEVMODE=%#o", 0666);
	return 0;
}

static ssize_t drivify_write(struct file *filp, const char __user *buf,
			     size_t count, loff_t *ppos)
{
	struct playlist_data *data;
	struct music_data music;
	int ret;

	data = container_of(filp->f_inode->i_cdev, struct playlist_data, cdev);
	if (!data) {
		pr_err("Unable to retrieve playlist data\n");
		return -ENODEV;
	}

	if (count != sizeof(struct music_data)) {
		pr_err("Invalid data size\n");
		return -EINVAL;
	}

	if (copy_from_user(&music, buf, count) != 0) {
		pr_err("Failed to copy data from user\n");
		return -EFAULT;
	}

	if (kfifo_len(data->playlist) >= MAX_PLAYLIST_SIZE * sizeof(music)) {
		pr_err("No more free place, playlist len = %d\n",
		       kfifo_len(data->playlist));
		return -ENOSPC;
	}

	ret = kfifo_in(data->playlist, &music, sizeof(struct music_data));
	if (ret != sizeof(struct music_data))
		return -EIO;

	pr_info("Added music: '%s' by '%s', duration: %u seconds\n",
		music.title, music.artist, music.duration);

	return count;
}

static const struct file_operations drivify_fops = {
	.owner = THIS_MODULE,
	.write = drivify_write,
};

static int switch_copy_probe(struct platform_device *pdev)
{
	struct priv *priv;
	int ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(struct priv), GFP_KERNEL);
	if (unlikely(!priv))
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);

	priv->io.dev = &pdev->dev;
	ret = setup_hw_irq(priv, pdev);
	if (ret != 0) {
		dev_err(priv->io.dev, "Failed to setup hw irq\n");
		return ret;
	}

	set_7_segment(0, priv);

	if (setup_timer_thread(priv) != 0) {
		dev_err(priv->io.dev, "Failed to setup timer thread\n");
		return -EINVAL;
	}

	priv->playlist_data.playlist =
		kmalloc(sizeof(struct kfifo), GFP_KERNEL);
	if (!priv->playlist_data.playlist) {
		pr_err("Failed to allocate memory for kfifo\n");
		ret = -ENOMEM;
		goto UNREGISTER_TIMER;
	}

	if (kfifo_alloc(priv->playlist_data.playlist,
			MAX_PLAYLIST_SIZE * sizeof(struct music_data),
			GFP_KERNEL)) {
		pr_err("Failed to initialize kfifo\n");
		ret = -ENOMEM;
		goto UNREGISTER_KFIFO;
	}

	if (register_chrdev_region(MAJMIN, 1, DEVICE_NAME)) {
		pr_err("Failed to register char device region\n");
		goto UNREGISTER_KFIFO;
	}

	priv->playlist_data.cl = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(priv->playlist_data.cl)) {
		pr_err("Failed to create class\n");
		ret = PTR_ERR(priv->playlist_data.cl);
		goto UNREGISTER_CHRDEV;
	}

	priv->playlist_data.cl->dev_uevent = playlist_uevent;
	cdev_init(&priv->playlist_data.cdev, &drivify_fops);
	ret = cdev_add(&priv->playlist_data.cdev, MAJMIN, 1);
	if (ret != 0) {
		pr_err("Failed to add cdev\n");
		goto UNREGISTER_CLASS;
	}

	priv->playlist_data.dev = device_create(priv->playlist_data.cl, NULL,
						MAJMIN, NULL, DEVICE_NAME);
	if (IS_ERR(priv->playlist_data.dev)) {
		pr_err("Failed to create device\n");
		ret = PTR_ERR(priv->playlist_data.dev);
		goto REMOVE_CDEV;
	}

	spin_lock_init(&priv->io.led_running_spinlock);
	spin_lock_init(&priv->io.segments_spinlock);

	return 0;

REMOVE_CDEV:
	cdev_del(&priv->playlist_data.cdev);
UNREGISTER_CLASS:
	class_destroy(priv->playlist_data.cl);
UNREGISTER_CHRDEV:
	unregister_chrdev_region(MAJMIN, 1);
UNREGISTER_KFIFO:
	kfifo_free(priv->playlist_data.playlist);
	kfree(priv->playlist_data.playlist);
UNREGISTER_TIMER:
	hrtimer_cancel(&priv->time.music_timer);
	kthread_stop(priv->time.display_thread);
	kfree(priv);
	return ret;
}

static int switch_copy_remove(struct platform_device *pdev)
{
	struct priv *priv = platform_get_drvdata(pdev);
	if (!priv) {
		pr_err("Failed to get private data\n");
		return -EINVAL;
	}

	dev_info(priv->io.dev, "Removing interrupt handler\n");

	// disable interrupts on hw
	iowrite8(0x0, priv->io.button_interrupt_mask);

	// clear 7seg
	iowrite32(0, priv->io.segment1);

	hrtimer_cancel(&priv->time.music_timer);
	kthread_stop(priv->time.display_thread);
	device_destroy(priv->playlist_data.cl, MAJMIN);
	cdev_del(&priv->playlist_data.cdev);
	class_destroy(priv->playlist_data.cl);
	unregister_chrdev_region(MAJMIN, 1);

	kfifo_free(priv->playlist_data.playlist);
	kfree(priv->playlist_data.playlist);
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
