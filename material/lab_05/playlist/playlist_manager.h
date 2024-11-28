#ifndef PLAYLIST_MANAGER_H
#define PLAYLIST_MANAGER_H

#include <linux/types.h>
#include "driver_types.h"

/*
* This function is used to manage the cycle of the playlist depending on the current private data. 
* If the current music is finished or the next music is requested, the next music is played.
* If the playlist is empty, the current music is set to NULL.
* This function managed the display of the current music and the time of the music.
*/
void playlist_cycle(struct priv *priv);

#endif // PLAYLIST_MANAGER_H
