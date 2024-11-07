// SPDX-License-Identifier: GPL-2.0
/*
 * REDS-adder driver, v0.1, 22.07.2022
 *
 * This is a very simplified driver, used to show you how a driver is structured
 * and to let you play a bit by interacting with the device using devmem2 and
 * observing the corresponding messages output by dmesg.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/of.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

/*
 * Offsets for the registers detailed in the documentation.
 */
#define ID_REG_OFF	 0x00
#define INCR_REG_OFF	 0x04
#define VALUE_REG_OFF	 0x08
#define INIT_REG_OFF	 0x0C
#define THRESH_REG_OFF	 0x10
#define IRQ_MASK_REG_OFF 0x80
#define IRQ_CAPT_REG_OFF 0x84

/*
 * Some constants that come handy when manipulating the registers.
 * We could have hardcoded them in the code, but that could have backfired:
 * if, for instance, the customer changes its mind (happens 99% of the time) and
 * wants two possibile initializations, or if the hardware engineer discovers
 * that a bug requires that the value 0x02 is written in the re-initialization
 * register (instead of 0x01), then we might be forced to pass through the whole
 * code base fixing this (surely forgetting the fix it somewhere, and thus
 * introducing a very subtle and difficult-to-catch bug).
 */
/* Reinitialize the counter. */
#define REINIT_CNT	 0x01
/*
 * Acknowledge a received interrupt (resetting the counter of the interrupt
 * register).
 */
#define ACK_IRQ		 0x01
/* Default threshold for interrupt triggering. */
#define DEFAULT_THR	 0x03
/* Disable the interrupt. */
#define INT_DISABLE	 0x00
/* Enable the interrupt. */
#define INT_ENABLE	 0x01
/* Disable the increments. */
#define INCR_DISABLE	 0x00
/* Enable the increments. */
#define INCR_ENABLE	 0x01

/*
 * @struct priv
 * @brief Private data for our driver.
 *
 * We will store here all the pointers and informations that are useful throughout
 * our driver. This information can be retrieved in the different functions of the
 * driver, thus allowing us to use the information that we stored there.
 *
 * @var priv::mem_ptr
 * Pointer to the ioremap()ed memory.
 * @var priv::irq_num
 * IRQ number, retrieved from the DT.
 * @var priv::dev
 * Pointer to our device (will be useful when printing out messages).
 */
struct priv {
	void *mem_ptr;
	int irq_num;
	struct device *dev;
};

/*
 * !!!! WARNING !!!!
 * In the functions below, why do we divide the offset by 4 ???
 * 'priv->mem_ptr' is a 'void *'-type pointer. According to the C11 standard,
 * pointer arithmetic on 'void *' is undefined (therefore Undefined Behavior),
 * but GCC chose to treat this kind of pointers as pointers to characters (thus,
 * bytes).
 * We are, however, casting this pointer to an 'int *'-type pointer, the integer
 * pointer arithmetic applies and thus adding '1' to a pointer moves it by 4
 * bytes.
 * The offsets given in the documentation are specified in bytes, so we have to
 * divide them by '4' to get the corresponding integer offset.
 * The advantage of having two dedicated functions for I/O is that we can rely on
 * them everywhere in our code; having ioread()/iowrite() in our driver instead is
 * not wrong, but we might forget that cast/division/offset/... and then spend a
 * day wondering why our peripheral behaves so strangely...
 */

/*
 * @brief Read a register from the REDS-adder device.
 *
 * @param priv: pointer to driver's private data
 * @param reg_offset: offset (in bytes) of the desired register
 *
 * @return: value read from the specified register.
 *
 * @note: this function is not used in this version of the driver (and, therefore,
 * the compiler will rightfully complain!). It is shown here nonetheless for
 * completeness/symmetry ;)
 */
static int ra_read(struct priv const *const priv, int const reg_offset)
{
	/*
	 * Assertions that will print a stacktrace when the condition in
	 * parentheses is true. They will alert us if we, by accident, were to
	 * call a ra_read() before having a valid pointer to the mapped registers.
	 */
	WARN_ON(priv == NULL);
	WARN_ON(priv->mem_ptr == NULL);

	/*
	 * As discussed above, the casting on 'priv->mem_ptr' is important. If you
	 * remove it, ugly things will happen...
	 */
	return ioread32((int *)priv->mem_ptr + (reg_offset / 4));
}

/*
 * @brief Write an integer value to a register of the REDS-adder device.
 *
 * @param priv: pointer to driver's private data
 * @param reg_offset: offset (in bytes) of the desired register
 * @param value: value that has to be written
 */
static void ra_write(struct priv const *const priv, int const reg_offset,
		     int const value)
{
	/*
	 * Assertions that will print a stacktrace when the condition in
	 * parentheses is true. They will alert us if we, by accident, were to
	 * call a ra_write() before having a valid pointer to the mapped
	 * registers.
	 */
	WARN_ON(priv == NULL);
	WARN_ON(priv->mem_ptr == NULL);

	dev_info(priv->dev,
		 "%s called with offset = 0x%x, value = %d\n",
		 __func__, reg_offset, value);

	/*
	 * As discussed above, the casting on 'priv->mem_ptr' is important. If you
	 * remove it, ugly things will happen...
	 */
	iowrite32(value, (int *)priv->mem_ptr + (reg_offset / 4));
}

/*
 * @brief Display the value of a string entry in the DT.
 *
 * Take a node in the DT and a property name, and display a string that shows the
 * value of that property. If the given property cannot be found in the node,
 * nothing is displayed.
 *
 * @param pdev: pointer to the platform device (used to customize the displayed
 * message)
 * @param dt_node: pointer to the DT node
 * @param node_name: name of the node whose value has to be displayed
 */
static void print_str_dt_prop(struct platform_device const *const pdev,
			      struct device_node const *const dt_node,
			      char const *const node_name)
{
	char const *value;

	if (of_property_read_string(dt_node, node_name, &value) == 0)
		dev_info(&pdev->dev, "\t%s = \"%s\"\n", node_name, value);
}

/*
 * @brief IRQ handler.
 *
 * React to the interrupts received from the board. For this tutorial we just have
 * to reset the counter to the initial value.
 *
 * @param irq: interrupt's numerical value (the same used in devm_request_irq())
 * @param dev_id: pointer to the variable passed as last parameter in the
 * devm_request_irq() function
 *
 * @returns: IRQ_HANDLED, since it always deals successfully with the interrupt
 */
irq_handler_t irq_handler(int irq, void *dev_id)
{
	/* We cast back the parameter to get our private data. */
	struct priv *priv = (struct priv *)dev_id;

	dev_info(priv->dev,
		 "called %s --- shouldn't print in an interrupt in real life, but this useful for debugging/understanding\n",
		 __func__);

	/* Reinitialize the counter and acknowledge the interrupt. */
	ra_write(priv, INIT_REG_OFF, REINIT_CNT);
	ra_write(priv, IRQ_CAPT_REG_OFF, ACK_IRQ);

	/*
	 * We successfully handled the interrupt, so we inform the kernel that
	 * we're ok.
	 */
	return (irq_handler_t)IRQ_HANDLED;
}

/*
 * @brief Driver's probe function.
 *
 * Since we have no init() function, all the initializations we require are made
 * here. Proper error checking is mandatory !
 * If this function fails, the module will be left in a sort of "zombie" state --
 * that is, it will still be loaded (we could see it with 'lsmod') but it won't do
 * much.
 *
 * @param pdev: pointer to platform device's structure
 *
 * @return: 0 when we were able to successfully initialize our driver, the failing
 * operation's error code otherwise.
 */
static int reds_adder_probe(struct platform_device *pdev)
{
	/*
	 * This will be our private data structure, used to share the variables
	 * we need for our job with all the other functions of the driver.
	 * NO, global variables are NOT allowed for that !
	 */
	struct priv *priv;
	/*
	 * Pointer to our entry in the DT -- just to show you a bit some DT
	 * manipulations, nothing really useful for our driver's purposes.
	 */
	struct device_node *dt_node;
	/* DT entry referring to the memory region for registers. */
	struct resource *mem_info;

	/* This variable will store our return codes. */
	int rc;

	/*
	 * Read the node corresponding to the REDS-adder from the DT and then
	 * display to the user our compatible string. Of course, I wouldn't do
	 * this in a real driver, but it is important to know that we can interact
	 * with the DT to retrieve useful information from it. We could, for
	 * instance, define some parameters in the DT and avoid having to
	 * recompile the whole driver each time (though we would have to recompile
	 * the DT).
	 */
	dt_node = of_find_node_by_path("/reds-adder");
	if (!dt_node) {
		dev_err(&pdev->dev,
			"Cannot find DT node -- who called me ??\n");
		rc = -ENODEV;
		goto return_fail;
	}
	dev_info(&pdev->dev,
		 "probe() - I have been called because the following line is in the DT\n");
	print_str_dt_prop(pdev, dt_node, "compatible");

	/*
	 * Allocate the memory for our private data.
	 * If this call fails, something's *very* wrong, but we have to check
	 * anyway. The 'unlikely()' here is a hint for the compiler for
	 * optimization purposes (if the memory allocation fails, we have much
	 * bigger problems than "oh no, I'm going to get the performance hit of
	 * backtracking my subsequent commands...").
	 * We use kzalloc() instead of kmalloc() since zeroing out memory is
	 * always best  (if we can afford it).
	 */
	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (unlikely(!priv)) {
		rc = -ENOMEM;
		goto return_fail;
	}

	dev_info(&pdev->dev, "Memory for the private data allocated\n");

	/*
	 * This call stores the pointer to our private data in the platform
	 * device. This allows to retrieve this pointer in functions that we do
	 * not call directly (where we could pass this pointer as a parameter),
	 * such as the module's remove function.
	 */
	platform_set_drvdata(pdev, priv);
	/*
	 * We sometimes need a pointer to the device (e.g., for printing messages
	 * with dev_X()), and so we store it in our private data to ensure that
	 * we have it in whatever function we end up with.
	 */
	priv->dev = &pdev->dev;

	/* Retrieve the address of the register's region from the DT. */
	mem_info = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (unlikely(!mem_info)) {
		dev_err(&pdev->dev,
			"Failed to get memory resource from device tree !\n");
		rc = -EINVAL;
		/*
		 * Since we have used the devm_kmalloc, we do not need to free
		 * our memory anymore --- we can just bail out.
		 */
		goto return_fail;
	}

	dev_info(&pdev->dev,
		 "Successfully retrieved the IOMEM address from the DT\n");

	/*
	 * The address we have just retrieved is a physical (IO) address, we
	 * cannot access it directly so we have to map it to a virtual address.
	 */
	priv->mem_ptr = devm_ioremap_resource(priv->dev, mem_info);
	if (IS_ERR(priv->mem_ptr)) {
		dev_err(&pdev->dev, "Failed to map memory!\n");
		rc = PTR_ERR(priv->mem_ptr);
		goto return_fail;
	}

	dev_info(&pdev->dev, "IOMEM address remapped\n");

	/*
	 * Before enabling interrupts on the device, we register the function
	 * that is supposed to be called when one occurs.
	 */
	dev_info(&pdev->dev, "Registering interrupt handler\n");
	/* Retrieve the IRQ number from the DT. */
	priv->irq_num = platform_get_irq(pdev, 0);
	if (priv->irq_num < 0) {
		dev_err(&pdev->dev,
			"Failed to get interrupt resource from device tree!\n");
		rc = -EINVAL;
		goto return_fail;
	}
	/* Register the ISR function associated with the interrupt. */
	rc = devm_request_irq(&pdev->dev, /* Our device */
			      priv->irq_num, /* IRQ number */
			      (irq_handler_t)irq_handler, /* ISR */
			      IRQF_SHARED, /* Flags */
			      "reds_adder_irq_handler", /* Name in /proc/interrupts */
			      (void *)priv /* Used to identify the device
					    * and to allow us access to
					    * private data in the IRQ
					    * handler
					    */
	);
	if (rc != 0) {
		dev_err(&pdev->dev,
			"REDS-adder_irq_handler: cannot register IRQ, error code: %d\n",
			rc);
		goto return_fail;
	}

	dev_info(&pdev->dev, "Successfully registered the interrupt handler\n");

	dev_info(&pdev->dev,
		 "Setting up initial values for the registers...\n");

	/*
	 * Since we have access to the device's registers, we can initialise
	 * the device. In particular, we can initialise the threshold for
	 * interrupt triggering and enable the interrupts.
	 * We also ensure that the increment register is set to '1' (we do not
	 * trust these hw people that much ;P).
	 */
	ra_write(priv, THRESH_REG_OFF, DEFAULT_THR);
	ra_write(priv, IRQ_MASK_REG_OFF, INT_ENABLE);
	ra_write(priv, INCR_REG_OFF, INCR_ENABLE);

	dev_info(&pdev->dev, "REDS-adder ready !\n");

	return 0;

return_fail:
	return rc;
}

/*
 * @brief Remove the module from the kernel, cleaning up the allocated structures.
 *
 * @param pdev: pointer to platform driver's structure
 *
 * @return: 0.
 */
static int reds_adder_remove(struct platform_device *pdev)
{
	/*
	 * Since we have given the kernel the pointer to the private data in the
	 * probe() function by using:
	 * platform_set_drvdata(pdev, priv);
	 * we can now retrieve it from the platform_device structure and use it
	 * to get the pointers to the resources we have to free.
	 * We are not calling directly the remove() function, therefore the only
	 * other way we would have to get these values is by using a global
	 * variable <-- which would be bad bad bad bad, the karma will hit you
	 * hard if you use global variables !!!
	 */
	struct priv *priv = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "Disabling interrupts\n");

	/* Disable further interrupts. */
	ra_write(priv, IRQ_MASK_REG_OFF, INT_DISABLE);

	dev_info(&pdev->dev, "Driver removed\n");

	return 0;
}

/*
 * Here we list the identifiers that might appear in the DT for the devices that
 * are supported by this driver. At boot the kernel reads the list of devices on
 * the board from the DT, and once a module claims to be responsible for one of
 * these devices (via this compatible string), the kernel knows that it has to
 * call that driver's probe() function.
 */
static const struct of_device_id reds_adder_driver_id[] = {
	{ .compatible = "reds,reds-adder" },
	{ /* END */ },
};
MODULE_DEVICE_TABLE(of, reds_adder_driver_id);

/*
 * This structure gives the kernel a list of properties associated with this
 * module, for instance the name of the probe and remove functions.
 */
static struct platform_driver reds_adder_driver = {
	.probe = reds_adder_probe,
	.remove = reds_adder_remove,
	.driver = {
		.name = "reds-adder",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(reds_adder_driver_id),
	},
};

/*
 * We declare this as a platform driver. As a benefit, we get the init and exit
 * functions (that will take care of the register/unregister operations) for free.
 */
module_platform_driver(reds_adder_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("REDS");
MODULE_DESCRIPTION("REDS-adder driver v0.1");
