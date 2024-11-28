#include "playlist_manager.h"
#include "io_manager.h"
#include <linux/slab.h>

int instanciate_music_if_null(struct priv *priv)
{
	if (!priv->playlist_data.current_music) {
		priv->playlist_data.current_music =
			kmalloc(sizeof(struct music_data), GFP_KERNEL);
		if (!priv->playlist_data.current_music) {
			pr_err("Failed to allocate memory for music data\n");
			return -ENOMEM;
		}
	}
	return 0;
}

int get_next_music_from_queue(struct priv *priv)
{
	if (kfifo_out(priv->playlist_data.playlist,
		      priv->playlist_data.current_music,
		      sizeof(struct music_data)) != sizeof(struct music_data)) {
		pr_err("Failed to get music data from playlist\n");
		kfree(priv->playlist_data.current_music);
		priv->playlist_data.current_music = NULL;
		return -EINVAL;
	}
	return 0;
}

int next_music(struct priv *priv)
{
	if (instanciate_music_if_null(priv))
		return -ENOMEM;

	if (get_next_music_from_queue(priv))
		return -EINVAL;

	pr_info("Playing music: '%s' by '%s', duration: %u seconds\n",
		priv->playlist_data.current_music->title,
		priv->playlist_data.current_music->artist,
		priv->playlist_data.current_music->duration);

	priv->time.current_time = 0;
	return 0;
}

bool should_switch_music(struct priv *priv)
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
			set_running_led(false, priv);
		}
		priv->playlist_data.next_music_requested = false;
	}
	set_time_segment(priv->time.current_time, priv);
	priv->time.current_time++;
}
