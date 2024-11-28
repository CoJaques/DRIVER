#ifndef TIMER_THREAD_MANAGER_H
#define TIMER_THREAD_MANAGER_H

#include <linux/hrtimer.h>
#include "driver_types.h"

int setup_timer_thread(struct priv *priv);
void cleanup_timer_thread(struct priv *priv);
enum hrtimer_restart timer_callback(struct hrtimer *timer);
int display_thread_func(void *data);

#endif // TIMER_THREAD_MANAGER_H
