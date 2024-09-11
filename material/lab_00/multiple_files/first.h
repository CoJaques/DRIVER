#include <stdio.h>

#include "second.h"

#define FIRST_NAME "first"

int id1 = 1;

void first()
{
	printf("Hello! I am %s (id %d) and my colleague is %s\n", FIRST_NAME, id1, SECOND_NAME);
}
