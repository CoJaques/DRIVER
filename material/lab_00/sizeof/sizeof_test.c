#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

char str_out[] = "my nice, quite long string (for sure more than 8 bytes...)";

void print_size(char str_array[])
{
	int int_var;
	float float_var;
	double double_var;

	unsigned char char_var;
	char *str_ptr = "my nice, quite long string (for sure more than 8 bytes...)";

	int *my_array;

	printf("sizeof(int_var) = %ld, sizeof(int) = %ld\n"
	       "sizeof(float_var) = %ld, sizeof(float) = %ld\n"
	       "sizeof(double_var) = %ld, sizeof(double) = %ld\n"
	       "sizeof(char_var) = %ld, sizeof(unsigned char) = %ld\n"
	       "sizeof(str_ptr) = %ld, sizeof(char *) = %ld\n"
	       "sizeof(str_array) = %ld\nsizeof(str_out) = %ld\n",
	       sizeof(int_var), sizeof(uint32_t),
	       sizeof(float_var), sizeof(float),
	       sizeof(double_var), sizeof(double),
	       sizeof(char_var), sizeof(unsigned char),
	       sizeof(str_ptr), sizeof(char *),
	       sizeof(str_array), sizeof(str_out));

	printf("Of course, I can trick the function sizeof() with the appropriate cast...\n"
	       "For instance, sizeof((char)double_var) = %ld\n\n",
	       sizeof((char)double_var));

	my_array = malloc(sizeof(*my_array) * 10);
	printf("For the dynamic allocation, the correct way to do the cast is the following one:\n"
	       "int *my_array = malloc(sizeof(*my_array) * 10);\n"
	       "Why? Should you change the type of 'my_array' in the declaration but not\n"
	       "in the malloc, then the memory allocated by:\n"
	       "int *my_array = malloc(sizeof(int) * 10);\n"
	       "would have the wrong size.\n\n"
	       "Call result: %p\n" \
	       "malloc() returns a void* but no need to cast it explicitly\n" \
	       "(but you do have to cast it for printing the resulting pointer)\n\n",
	       (void *)my_array);

	printf("sizeof(my_array) = %ld, sizeof(*my_array) = %ld\n",
	       sizeof(my_array), sizeof(*my_array));

	free(my_array);
}

int main()
{
	print_size(str_out);

	return 0;
}
