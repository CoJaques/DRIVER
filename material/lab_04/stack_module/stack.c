// SPDX-License-Identifier: GPL-2.0
/*
 * Stack file
 */

#include "linux/device/class.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/slab.h>

#include <linux/string.h>

#define MAJOR_NUM   98
#define MAJMIN	    MKDEV(MAJOR_NUM, 0)
#define DEVICE_NAME "stack"

struct stack_node {
	struct list_head list;
	uint32_t value;
};

struct stack_device {
	struct cdev cdev;
	struct class *cl;
	struct list_head head;
	struct list_head *tail;
};

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
	ssize_t nb_values;

	if (count % sizeof(uint32_t) != 0)
		return -EINVAL;

	nb_values = count / sizeof(uint32_t);
	uint32_t *new_values = kmalloc(count, GFP_KERNEL);

	if (new_values == NULL)
		return -ENOMEM;

	if (copy_from_user(new_values, buf, count) != 0) {
		kfree(new_values);
		dev_err(cl->dev, "Failed to copy buffer from user\n");
		return -EFAULT;
	}

	struct stack_node *new_elem;
	for (ssize_t i = 0; i < nb_values; i++) {
		new_elem = kmalloc(sizeof(struct stack_node), GFP_KERNEL);
		if (new_elem == NULL)
			return -ENOMEM;

		new_elem->value = new_values[i];
		list_add(&new_elem->list, &stack_head);
	}

	current_node = new_elem;

	return count;
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
	struct stack_device *stack_dev;

	// Allocate memory for the device
	stack_dev = kzalloc(sizeof(struct stack_device), GFP_KERNEL);
	if (stack_dev == NULL) {
		pr_err("Stack: Error allocating memory for the device\n");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&stack_dev->head);
	stack_dev->tail = &stack_dev->head;

	// Register the device
	err = register_chrdev_region(MAJMIN, 1, DEVICE_NAME);
	if (err != 0) {
		pr_err("Stack: Registering char device failed\n");
		kfree(stack_dev);
		return err;
	}

	stack_dev->cl = class_create(THIS_MODULE, DEVICE_NAME);
	if (stack_dev->cl == NULL) {
		pr_err("Stack: Error creating class\n");
		unregister_chrdev_region(MAJMIN, 1);
		kfree(stack_dev);
		return -1;
	}
	stack_dev->cl->dev_uevent = stack_uevent;
	struct device *dev =
		device_create(stack_dev->cl, NULL, MAJMIN, NULL, DEVICE_NAME);
	if (dev == NULL) {
		pr_err("Stack: Error creating device\n");
		class_destroy(stack_dev->cl);
		unregister_chrdev_region(MAJMIN, 1);
		kfree(stack_dev);
		return -1;
	}

	dev_set_drvdata(dev, stack_dev);

	cdev_init(&stack_dev->cdev, &stack_fops);

	err = cdev_add(&stack_dev->cdev, MAJMIN, 1);
	if (err < 0) {
		pr_err("Stack: Adding char device failed\n");
		device_destroy(stack_dev->cl, MAJMIN);
		class_destroy(stack_dev->cl);
		unregister_chrdev_region(MAJMIN, 1);
		kfree(stack_dev);
		return err;
	}

	pr_info("Stack ready!\n");
	return 0;
}

static void __exit stack_exit(void)
{
	struct device *dev;
	struct stack_device *stack_dev;
	struct stack_node *node, *tmp;

	dev = class_find_device_by_name(NULL, DEVICE_NAME);
	if (!dev) {
		pr_err("Stack: Device not found\n");
		return;
	}

	stack_dev = dev_get_drvdata(dev);
	if (!stack_dev) {
		pr_err("Stack: Unable to retrieve stack_device\n");
		return;
	}

	list_for_each_entry_safe(node, tmp, &stack_dev->head, list) {
		list_del(&node->list);
		kfree(node);
	}

	cdev_del(&stack_dev->cdev);
	device_destroy(stack_dev->cl, MAJMIN);
	class_destroy(stack_dev->cl);
	unregister_chrdev_region(MAJMIN, 1);

	kfree(stack_dev);

	pr_info("Stack cleaned up successfully!\n");
}

MODULE_AUTHOR("REDS");
MODULE_LICENSE("GPL");

module_init(stack_init);
module_exit(stack_exit);
