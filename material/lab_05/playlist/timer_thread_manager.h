#ifndef TIMER_THREAD_MANAGER_H
#define TIMER_THREAD_MANAGER_H

#include <linux/hrtimer.h>
#include "driver_types.h"

/*
 * This function is called when the module is loaded and it sets up the timer and the thread.
 */
int setup_timer_thread(struct priv *priv);

/*
 * This function is called when the module is removed and it stops the timer and the thread.
 */
void cleanup_timer_thread(struct priv *priv);

#endif // TIMER_THREAD_MANAGER_H
