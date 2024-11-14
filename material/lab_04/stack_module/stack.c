// SPDX-License-Identifier: GPL-2.0
/*
 * Stack file
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include <linux/string.h>

#define MAJOR_NUM 98
#define MAJMIN MKDEV(MAJOR_NUM, 0)
#define DEVICE_NAME "stack"

static struct cdev cdev;
static struct class *cl;

/**
 * @brief Pop and return latest added element of the stack.
 *
 * @param filp pointer to the file descriptor in use
 * @param buf destination buffer in user space
 * @param count maximum number of byte to read
 * @param ppos ignored
 *
 * @return Actual number of bytes read from internal buffer,
 *         or a negative error code
 */
static ssize_t stack_read(struct file *filp, char __user *buf, size_t count,
			  loff_t *ppos)
{
	return 0;
}

/**
 * @brief Push the element on the stack
 *
 * @param filp pointer to the file descriptor in use
 * @param buf source buffer in user space
 * @param count number of byte to write in the buffer
 * @param ppos ignored
 *
 * @return Actual number of bytes writen to internal buffer,
 *         or a negative error code
 */
static ssize_t stack_write(struct file *filp, const char __user *buf,
			   size_t count, loff_t *ppos)
{
	return 0;
}

/**
 * @brief uevent callback to set the permission on the device file
 *
 * @param dev pointer to the device
 * @param env uevent environnement corresponding to the device
 */
static int stack_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	// Set the permissions of the device file
	add_uevent_var(env, "DEVMODE=%#o", 0666);
	return 0;
}

static const struct file_operations stack_fops = {
	.owner = THIS_MODULE,
	.read = stack_read,
	.write = stack_write,
};

static int __init stack_init(void)
{
	int err;

	// Register the device
	err = register_chrdev_region(MAJMIN, 1, DEVICE_NAME);
	if (err != 0) {
		pr_err("Stack: Registering char device failed\n");
		return err;
	}

	cl = class_create(THIS_MODULE, DEVICE_NAME);
	if (cl == NULL) {
		pr_err("Stack: Error creating class\n");
		unregister_chrdev_region(MAJMIN, 1);
		return -1;
	}
	cl->dev_uevent = stack_uevent;

	if (device_create(cl, NULL, MAJMIN, NULL, DEVICE_NAME) == NULL) {
		pr_err("Stack: Error creating device\n");
		class_destroy(cl);
		unregister_chrdev_region(MAJMIN, 1);
		return -1;
	}

	cdev_init(&cdev, &stack_fops);
	err = cdev_add(&cdev, MAJMIN, 1);
	if (err < 0) {
		pr_err("Stack: Adding char device failed\n");
		device_destroy(cl, MAJMIN);
		class_destroy(cl);
		unregister_chrdev_region(MAJMIN, 1);
		return err;
	}

	pr_info("Stack ready!\n");
	return 0;
}

static void __exit stack_exit(void)
{
	// Unregister the device
	cdev_del(&cdev);
	device_destroy(cl, MAJMIN);
	class_destroy(cl);
	unregister_chrdev_region(MAJMIN, 1);

	pr_info("Stack done!\n");
}

MODULE_AUTHOR("REDS");
MODULE_LICENSE("GPL");

module_init(stack_init);
module_exit(stack_exit);
