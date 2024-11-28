#ifndef PLAYLIST_MANAGER_H
#define PLAYLIST_MANAGER_H

#include <linux/types.h>
#include "driver_types.h"

int instanciate_music_if_null(struct priv *priv);
int get_next_music_from_queue(struct priv *priv);
int next_music(struct priv *priv);
bool should_switch_music(struct priv *priv);
void playlist_cycle(struct priv *priv);

#endif // PLAYLIST_MANAGER_H
