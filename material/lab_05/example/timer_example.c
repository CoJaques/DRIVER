#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/timer.h>

/**
 * struct priv - Private data for the module to show how from_timer works
 *
 * @id:		An id that will be shown by timer_hanlder
 * @my_timer:	The timer_list struct used for the timer
 */
struct priv {
	int id;
	struct timer_list my_timer;
};

static struct priv data;

/**
 * timer_handler - Timer handler that show a simple message and reschedule the timer.
 *
 * @timer:	Pointer to the timer_list structure.
 */
static void timer_handler(struct timer_list *timer)
{
	struct priv *priv_data = from_timer(priv_data, timer, my_timer);

	pr_info("Timer handler function called, id is %d\n", priv_data->id);

	/* Reschedule the timer to run again after 1 second */
	mod_timer(timer, jiffies + msecs_to_jiffies(1000));
}

static int __init timer_example_init(void)
{
	pr_info("Timer example module loaded\n");

	data.id = 10;

	/* Initialize the timer */
	timer_setup(&data.my_timer, timer_handler, 0);

	/* Arm the timer to fire after 1 second */
	mod_timer(&data.my_timer, jiffies + msecs_to_jiffies(1000));

	return 0;
}

static void __exit timer_example_exit(void)
{
	/* Delete the timer */
	del_timer_sync(&data.my_timer);
	pr_info("Timer example module unloaded\n");
}

module_init(timer_example_init);
module_exit(timer_example_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("REDS");
MODULE_DESCRIPTION("A simple timer example");
