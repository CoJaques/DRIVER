// Le problème est lié au fait qu'il y a des inclusions circulaire entre first et second, l'utilisation
// des guars permet de régler le soucis.

#ifndef FIRST_H
#define FIRST_H

#include <stdio.h>

#include "second.h"

#define FIRST_NAME "first"

int id1 = 1;

void first()
{
	printf("Hello! I am %s (id %d) and my colleague is %s\n", FIRST_NAME,
	       id1, SECOND_NAME);
}
#endif // FIRST_H
