#include <stdio.h>

struct A {
	int b;
	char c;
};

int main()
{
	struct A firstStruct;
	firstStruct.b = 42;
	firstStruct.c = 'X';

	struct {
		int b;
		char c;
	} secondStruct;

	secondStruct.b = 99;
	secondStruct.c = 'Y';

	printf("First struct: b = %d, c = %c\n", firstStruct.b, firstStruct.c);

	printf("Second struct: b = %d, c = %c\n", secondStruct.b,
	       secondStruct.c);

	return 0;
}
