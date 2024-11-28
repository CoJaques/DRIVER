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

/*
 * This function is called every time the timer expires and run the threda used for playlist cycle.
 */
enum hrtimer_restart timer_callback(struct hrtimer *timer);

/*
*  This function define the behaviour of the thread that will manage the playlist cycle.
*/
int playlist_thread_func(void *data);

#endif // TIMER_THREAD_MANAGER_H
