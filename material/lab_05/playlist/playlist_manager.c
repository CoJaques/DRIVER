#include "playlist_manager.h"
#include "driver_types.h"
#include "io_manager.h"
#include <linux/slab.h>

static int instanciate_music_if_null(struct playlist_data *data)
{
	if (!data->current_music) {
		data->current_music =
			kmalloc(sizeof(struct music_data), GFP_KERNEL);
		if (!data->current_music) {
			pr_err("Failed to allocate memory for music data\n");
			return -ENOMEM;
		}
	}
	return 0;
}

static int get_next_music_from_queue(struct playlist_data *data)
{
	if (kfifo_out(data->playlist, data->current_music,
		      sizeof(struct music_data)) != sizeof(struct music_data)) {
		pr_err("Failed to get music data from playlist\n");
		kfree(data->current_music);
		data->current_music = NULL;
		return -EINVAL;
	}
	return 0;
}

static int next_music(struct priv *priv)
{
	if (instanciate_music_if_null(&priv->playlist_data))
		return -ENOMEM;

	if (get_next_music_from_queue(&priv->playlist_data))
		return -EINVAL;

	pr_info("Playing music: '%s' by '%s', duration: %u seconds\n",
		priv->playlist_data.current_music->title,
		priv->playlist_data.current_music->artist,
		priv->playlist_data.current_music->duration);

	atomic_set(&priv->time.current_time, 0);
	return 0;
}

static bool should_switch_music(struct priv *priv)
{
	return !priv->playlist_data.current_music ||
	       atomic_read(&priv->time.current_time) >=
		       priv->playlist_data.current_music->duration ||
	       priv->playlist_data.next_music_requested;
}

void playlist_cycle(struct priv *priv)
{
	if (should_switch_music(priv)) {
		if (kfifo_is_empty(priv->playlist_data.playlist) ||
		    next_music(priv)) {
			kfree(priv->playlist_data.current_music);
			priv->playlist_data.current_music = NULL;
			atomic_set(&priv->time.current_time, 0);
			atomic_set(&priv->is_playing, false);
			set_running_led(false, &priv->io);
		}
		priv->playlist_data.next_music_requested = false;
	} else {
		atomic_inc(&priv->time.current_time);
	}

	set_time_segment(atomic_read(&priv->time.current_time), &priv->io);
}

void handle_play_pause(bool play, struct priv *priv)
{
	bool kfifo_empty = kfifo_is_empty(priv->playlist_data.playlist);
	bool current_music_null = priv->playlist_data.current_music == NULL;

	atomic_set(&priv->is_playing, play);

	if (play && (!current_music_null || !kfifo_empty)) {
		priv->playlist_data.next_music_requested = false;
		hrtimer_start(&priv->time.music_timer, ktime_set(1, 0),
			      HRTIMER_MODE_REL);
		set_running_led(true, &priv->io);
	} else {
		hrtimer_cancel(&priv->time.music_timer);
		set_running_led(false, &priv->io);
	}
}
