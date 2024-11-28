// COLIN JAQUES

#include "linux/dev_printk.h"
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

#include "irq_manager.h"
#include "timer_thread_manager.h"
#include "playlist.h"
#include "driver_types.h"
#include "io_manager.h"

#define CLEANUP_ON_ERROR(action, label, dev, message) \
	do {                                          \
		if ((ret = action) < 0) {             \
			dev_err(dev, message);        \
			goto label;                   \
		}                                     \
	} while (0)

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
	CLEANUP_ON_ERROR(setup_hw_irq(priv, pdev), UNREGISTER_TIMER,
			 priv->io.dev, "Failed to setup hw irq\n");

	set_7_segment(0, priv);

	CLEANUP_ON_ERROR(setup_timer_thread(priv), UNREGISTER_TIMER,
			 priv->io.dev, "Failed to setup timer thread\n");

	priv->playlist_data.playlist =
		kmalloc(sizeof(struct kfifo), GFP_KERNEL);
	if (!priv->playlist_data.playlist) {
		pr_err("Failed to allocate memory for kfifo\n");
		ret = -ENOMEM;
		goto UNREGISTER_TIMER;
	}

	CLEANUP_ON_ERROR(
		kfifo_alloc(priv->playlist_data.playlist,
			    MAX_PLAYLIST_SIZE * sizeof(struct music_data),
			    GFP_KERNEL),
		UNREGISTER_KFIFO, priv->io.dev, "Failed to initialize kfifo\n");

	CLEANUP_ON_ERROR(register_chrdev_region(MAJMIN, 1, DEVICE_NAME),
			 UNREGISTER_KFIFO, priv->io.dev,
			 "Failed to register char device region\n");

	priv->playlist_data.cl = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(priv->playlist_data.cl)) {
		pr_err("Failed to create class\n");
		ret = PTR_ERR(priv->playlist_data.cl);
		goto UNREGISTER_CHRDEV;
	}

	priv->playlist_data.cl->dev_uevent = playlist_uevent;
	cdev_init(&priv->playlist_data.cdev, &drivify_fops);
	CLEANUP_ON_ERROR(cdev_add(&priv->playlist_data.cdev, MAJMIN, 1),
			 UNREGISTER_CLASS, priv->io.dev,
			 "Failed to add cdev\n");

	priv->playlist_data.dev = device_create(priv->playlist_data.cl, NULL,
						MAJMIN, NULL, DEVICE_NAME);
	if (IS_ERR(priv->playlist_data.dev)) {
		pr_err("Failed to create device\n");
		ret = PTR_ERR(priv->playlist_data.dev);
		goto REMOVE_CDEV;
	}

	spin_lock_init(&priv->io.led_running_spinlock);
	spin_lock_init(&priv->io.segments_spinlock);

	dev_info(priv->io.dev, "Playlist driver initialized\n");

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

	return 0;
}

static const struct of_device_id switch_copy_driver_id[] = {
	{ .compatible = "drv2024" },
	{ /* sentinel */ }
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
MODULE_AUTHOR("Colin Jaques");
MODULE_DESCRIPTION("Bleeding edge playlist driver");
