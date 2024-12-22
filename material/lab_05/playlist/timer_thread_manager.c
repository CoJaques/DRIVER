#include "timer_thread_manager.h"
#include "playlist_manager.h"

#include <linux/kthread.h>

static enum hrtimer_restart timer_callback(struct hrtimer *timer)
{
	struct priv *priv = container_of(timer, struct priv, time.music_timer);

	if (atomic_read(&priv->is_playing)) {
		complete(&priv->time.display_thread_completion);
		hrtimer_forward_now(timer, ktime_set(1, 0));
		return HRTIMER_RESTART;
	}
	return HRTIMER_NORESTART;
}

static int playlist_thread_func(void *data)
{
	struct priv *priv = (struct priv *)data;

	while (!kthread_should_stop()) {
		if (atomic_read(&priv->is_playing))
			playlist_cycle(priv);
		wait_for_completion_interruptible(
			&priv->time.display_thread_completion);
	}

	return 0;
}

int setup_timer_thread(struct priv *priv)
{
	init_completion(&priv->time.display_thread_completion);
	hrtimer_init(&priv->time.music_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	priv->time.music_timer.function = timer_callback;

	priv->time.display_thread =
		kthread_run(playlist_thread_func, priv, "display_thread");
	return 0;
}

void cleanup_timer_thread(struct priv *priv)
{
	kthread_stop(priv->time.display_thread);
	hrtimer_cancel(&priv->time.music_timer);
}
