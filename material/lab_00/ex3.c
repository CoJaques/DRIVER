/*******************************************************************
 * Copyright (c) 2024 Colin Jaques
 *******************************************************************/

#include <stdio.h>

int main()
{
	unsigned int value = 0xBEEFCAFE;

	unsigned char *byte_ptr = (unsigned char *)&value;

	printf("Reading the value 0xBEEFCAFE byte by byte:\n");
	for (int i = sizeof(value) - 1; i >= 0; i--) {
		printf("Byte %d: 0x%02X\n", i, *(byte_ptr + i));
	}

	if (byte_ptr[0] == 0xFE) {
		printf("Your system is little-endian.\n");
	} else if (byte_ptr[0] == 0xBE) {
		printf("Your system is big-endian.\n");
	} else {
		printf("Unknown endianness.\n");
	}

	return 0;
}
