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

	priv->time.current_time = 0;
	return 0;
}

static bool should_switch_music(struct priv *priv)
{
	return !priv->playlist_data.current_music ||
	       priv->time.current_time >=
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
			priv->time.current_time = 0;
			priv->is_playing = false;
			set_running_led(false, &priv->io);
		}
		priv->playlist_data.next_music_requested = false;
	}
	set_time_segment(priv->time.current_time, &priv->io);
	priv->time.current_time++;
}