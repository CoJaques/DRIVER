#include <stdio.h>

#include "first.h"
#include "second.h"

int main()
{
	printf("I am the main, and I wish to call my assistants %s and %s!\n",
	       FIRST_NAME, SECOND_NAME);
	first();
	second();

	return 0;
}
