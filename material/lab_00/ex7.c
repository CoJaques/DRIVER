/*******************************************************************
 * Copyright (c) 2024 Colin Jaques
 *******************************************************************/

#include <stdio.h>
#include <stddef.h>

#define container_of(ptr, type, member) \
	(type *)(void *)((char *)ptr - offsetof(type, member))

struct MyStruct {
	int a;
	double b;
	char c;
};

int main()
{
	struct __attribute__((packed)) {
		int a;
		double b;
		char c;
	} example;

	// Pointeurs vers les membres
	int *a_ptr = &example.a;
	double *b_ptr = &example.b;
	char *c_ptr = &example.c;

	// Utilisation de container_of pour retrouver la structure parente
	struct MyStruct *from_a = container_of(a_ptr, struct MyStruct, a);
	struct MyStruct *from_b = container_of(b_ptr, struct MyStruct, b);
	struct MyStruct *from_c = container_of(c_ptr, struct MyStruct, c);

	// Afficher les adresses
	printf("Adresse de base de la structure: %p\n", (void *)&example);
	printf("Adresse obtenue via 'a': %p\n", (void *)from_a);
	printf("Adresse obtenue via 'b': %p\n", (void *)from_b);
	printf("Adresse obtenue via 'c': %p\n", (void *)from_c);

	return 0;
}
