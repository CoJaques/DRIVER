/*******************************************************************
 * Copyright (c) 2024 Colin Jaques
 *******************************************************************/

#include <stdio.h>

union Data {
    unsigned int value;    
    unsigned char bytes[4];
};

int main() {
    union Data data;
    data.value = 0xBEEFCAFE;

    printf("Reading the value 0xBEEFCAFE byte by byte using a union:\n");
    for (size_t i = 0; i < sizeof(data.bytes); i++) {
        printf("Byte %lu: 0x%02X\n", i, data.bytes[i]);
    }

    if (data.bytes[0] == 0xFE) {
        printf("Your system is little-endian.\n");
    } else if (data.bytes[0] == 0xBE) {
        printf("Your system is big-endian.\n");
    } else {
        printf("Unknown endianness.\n");
    }

    return 0;
}
