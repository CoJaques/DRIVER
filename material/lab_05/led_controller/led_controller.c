#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/workqueue.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("REDS");
MODULE_DESCRIPTION("Led controller with multiple pattern");

#define LEDS_OFST 0x0

#define KEY_OFST 0x50
#define KEY_IRQ_EN_OFST (KEY_OFST + 0x8)
#define KEY_IRQ_EDGE_OFST (KEY_OFST + 0xC)

#define NB_LEDS 10
#define LEDS_MASK ((1 << NB_LEDS) - 1)

#define DEV_NAME "led_controller"

#define MOD_NOTHING 0
#define MOD_INC 1
#define MOD_DEC 2
#define MOD_ROT_LEFT 3
#define MOD_ROT_RIGHT 4

#define UPDATE_INTERVAL 2500

/**
 * struct priv - Private data for the device
 * @mem_ptr:	Pointer to the IO mapped memory.
 * @dev:	Pointer to the device.
 * @value:	Actual value displayed on the leds.
 * @mod:	Actual mod used to modify the value.
 * @work:	Delayed work used to update the value.
 */
struct priv {
	void *mem_ptr;
	struct device *dev;

	uint16_t value;
	uint8_t mod;
	struct delayed_work work;
};

/* Prototypes for sysfs callbacks */
static ssize_t mod_show(struct device *dev, struct device_attribute *attr,
			char *buf);
static ssize_t mod_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count);

static ssize_t value_show(struct device *dev, struct device_attribute *attr,
			  char *buf);
static ssize_t value_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count);

static DEVICE_ATTR_RW(mod);
static DEVICE_ATTR_RW(value);

static struct attribute *lc_attrs[] = {
	&dev_attr_mod.attr,
	&dev_attr_value.attr,
	NULL,
};

static struct attribute_group lc_attr_group = {
	.name = "config",
	.attrs = lc_attrs,
};

/**
 * lc_write - Write a value to a register in the IO mapped memory.
 * @priv:	Pointer to the private data of the device.
 * @reg_offset:	Offset of the register to write (in bytes).
 * @value:	Value to write to the register.
 */
static void lc_write(const struct priv *const priv, const int reg_offset,
		     const uint32_t value)
{
	// Assert that the pointers are not NULL, this should never happend
	WARN_ON(priv == NULL);
	WARN_ON(priv->mem_ptr == NULL);

	iowrite32(value,
		  (uint32_t *)priv->mem_ptr + reg_offset / sizeof(uint32_t));
}

/**
 * mod_show - Callback for the show operation on the mod attribute.
 *
 * @dev:	Pointer to the device structure.
 * @attr:	Pointer to the device attribute structure.
 * @buf:	Pointer to the buffer to write the read data to.
 * Return: The number of bytes written to the buffer.
 */
static ssize_t mod_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct priv *priv = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", priv->mod);
}

/**
 * mod_store - Callback for the store operation on the mod attribute.
 *
 * @dev:	Pointer to the device structure.
 * @attr:	Pointer to the device attribute structure.
 * @buf:	Pointer to the buffer to read the data from.
 * @count:	Number of bytes to read.
 * Return: The number of bytes read from the buffer.
 */
static ssize_t mod_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct priv *priv = dev_get_drvdata(dev);
	int rc;
	uint8_t new_mod;

	rc = kstrtou8(buf, 10, &new_mod);
	if (rc != 0) {
		dev_err(dev, "Failed to convert the value to an integer !\n");
		return rc;
	}

	if (new_mod > MOD_ROT_RIGHT) {
		return -EINVAL;
	}

	priv->mod = new_mod;
	return count;
}

/**
 * value_show - Callback for the show operation on the value attribute.
 *
 * @dev:	Pointer to the device structure.
 * @attr:	Pointer to the device attribute structure.
 * @buf:	Pointer to the buffer to write the read data to.
 * Return: The number of bytes written to the buffer.
 */
static ssize_t value_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct priv *priv = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%03x\n", priv->value);
}

/**
 * value_store - Callback for the store operation on the value attribute.
 *
 * @dev:	Pointer to the device structure.
 * @attr:	Pointer to the device attribute structure.
 * @buf:	Pointer to the buffer to read the data from.
 * @count:	Number of bytes to read.
 * Return: The number of bytes read from the buffer.
 */
static ssize_t value_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct priv *priv = dev_get_drvdata(dev);
	int rc;
	uint16_t new_val;

	rc = kstrtou16(buf, 16, &new_val);
	if (rc != 0) {
		dev_err(dev, "Failed to convert the value to an integer !\n");
		return rc;
	}

	if (new_val >= (1 << NB_LEDS)) {
		return -EINVAL;
	}

	priv->value = new_val;
	lc_write(priv, LEDS_OFST, priv->value);
	return count;
}

/**
 * work_handler - Work handler to update the leds based on current mod.
 *
 * @work:	Pointer to the work_struct.
 */
static void work_handler(struct work_struct *work)
{
	struct priv *priv = container_of(work, struct priv, work.work);

	// Execute the correct modification to the value
	switch (priv->mod) {
	case MOD_NOTHING:
		break;
	case MOD_INC:
		priv->value++;
		break;
	case MOD_DEC:
		priv->value--;
		break;
	case MOD_ROT_LEFT:
		priv->value = (priv->value << 1) |
			      (priv->value >> (NB_LEDS - 1));
		break;
	case MOD_ROT_RIGHT:
		priv->value = (priv->value >> 1) |
			      (priv->value << (NB_LEDS - 1));
		break;
	default:
		break;
	}
	priv->value = priv->value & LEDS_MASK;
	lc_write(priv, LEDS_OFST, priv->value);

	// Schedule next work iteration
	schedule_delayed_work(&priv->work, msecs_to_jiffies(UPDATE_INTERVAL));
}

/**
 * led_controller_probe - Probe function of the platform driver.
 * @pdev:	Pointer to the platform device structure.
 * Return: 0 on success, negative error code on failure.
 */
static int led_controller_probe(struct platform_device *pdev)
{
	struct priv *priv;
	int rc;

	/******* Allocate memory for private data *******/
	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (unlikely(!priv)) {
		dev_err(&pdev->dev,
			"Failed to allocate memory for private data\n");
		rc = -ENOMEM;
		goto return_fail;
	}

	// Set the driver data of the platform device to the private data
	platform_set_drvdata(pdev, priv);
	priv->dev = &pdev->dev;
	priv->value = 0;
	priv->mod = MOD_INC;

	/******* Setup memory region pointers *******/
	priv->mem_ptr = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->mem_ptr)) {
		dev_err(&pdev->dev, "Failed to remap memory");
		rc = PTR_ERR(priv->mem_ptr);
		goto return_fail;
	}

	/***** Setup sysfs *****/
	rc = sysfs_create_group(&pdev->dev.kobj, &lc_attr_group);
	if (rc != 0) {
		dev_err(priv->dev, "Error while creating the sysfs group\n");
		goto return_fail;
	}

	/*************** Setup registers ***************/
	// Turn off the leds
	lc_write(priv, LEDS_OFST, 0);

	/*************** Setup delayed work ***************/
	INIT_DELAYED_WORK(&priv->work, work_handler);
	schedule_delayed_work(&priv->work, msecs_to_jiffies(UPDATE_INTERVAL));

	dev_info(&pdev->dev, "led_controller probe successful!\n");

	return 0;

return_fail:
	return rc;
}

/**
 * led_controller_remove - Remove function of the platform driver.
 * @pdev:	Pointer to the platform device structure.
 * Return: 0 on success, negative error code on failure.
 */
static int led_controller_remove(struct platform_device *pdev)
{
	// Retrieve the private data from the platform device
	struct priv *priv = platform_get_drvdata(pdev);

	// Turn off the leds
	lc_write(priv, LEDS_OFST, 0);

	sysfs_remove_group(&pdev->dev.kobj, &lc_attr_group);

	// Delete the delayed work
	cancel_delayed_work_sync(&priv->work);

	dev_info(&pdev->dev, "led_controller remove successful!\n");

	return 0;
}

/* Instanciate the list of supported devices */
static const struct of_device_id led_controller_id[] = {
	{ .compatible = "drv2024" },
	{ /* END */ },
};
MODULE_DEVICE_TABLE(of, led_controller_id);

/* Instanciate the platform driver for this driver */
static struct platform_driver led_controller_driver = {
	.driver = {
		.name = "led_controller",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(led_controller_id),
	},
	.probe = led_controller_probe,
	.remove = led_controller_remove,
};

/*
 * As init and exit function only have to register and unregister the
 * platform driver, we can use this helper macros that will automatically
 * create the functions.
 */
module_platform_driver(led_controller_driver);
