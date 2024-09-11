#include <stdio.h>

#include "first.h"

#define SECOND_NAME "second"

int id2 = 2;

void second()
{
	printf("I am %s (id = %d) and I introduce no-one :(\n", SECOND_NAME, id2);
}
