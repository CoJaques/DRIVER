/*******************************************************************
 * Copyright (c) 2024 Colin Jaques
 *******************************************************************/

#include <stdio.h>
#include <stdint.h>

#define constant 0

int main()
{
	uint32_t value = 1;

	int shift;

#if constant
	shift = 42;
#else
	scanf("%d", &shift);
#endif

	uint32_t result = value << shift;
	printf("0x%08X << %d = 0x%08X\n", value, shift, result);

	return 0;
}