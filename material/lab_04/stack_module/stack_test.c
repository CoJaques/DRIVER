#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define ONE_BY_ONE_PUSH 16
#define ONE_BY_ONE_POP 4
#define ARRAY_POP 6
#define ARRAY_RAND 5
#define TMP_SIZE 256

int main(void)
{
	int fd;
	uint32_t tmp_array[TMP_SIZE];
	uint32_t rand_array[ARRAY_RAND];
	uint32_t tmp;
	uint32_t incremental_val = 0;
	uint32_t i;
	ssize_t ret;

	fd = open("/dev/stack", O_RDWR);
	if (fd < 0) {
		perror("stack_test");
		return EXIT_FAILURE;
	}

	printf("Testing emptyness.\n");
	ret = read(fd, &tmp, sizeof(tmp));
	if (ret != 0) {
		printf("Stack read returned %d but should be empty!\n", ret);
		return EXIT_FAILURE;
	}

	printf("Pushing 0 to %d one by one\n", ONE_BY_ONE_PUSH - 1);
	for (i = 0; i < ONE_BY_ONE_PUSH; i++) {
		if (write(fd, &incremental_val, sizeof(uint32_t)) !=
		    sizeof(uint32_t)) {
			printf("Error while writting %u\n", incremental_val);
			return EXIT_FAILURE;
		}

		incremental_val++;
	}
	// Correct the value, as it is incremented one more than necessary.
	incremental_val--;

	printf("Poping %d element one by one.\n", ONE_BY_ONE_POP);
	for (i = 0; i < ONE_BY_ONE_POP; i++) {
		if (read(fd, &tmp, sizeof(tmp)) != sizeof(tmp)) {
			printf("Error while poping Readed %d value\n", i);
			return EXIT_FAILURE;
		}

		if (tmp != incremental_val) {
			printf("Readed %d is false. Got %u, expected %u.\n", i,
			       tmp, incremental_val);
			return EXIT_FAILURE;
		}
		incremental_val--;
	}

	printf("Poping %d elements in one time.\n", ARRAY_POP);
	ret = read(fd, &tmp_array, sizeof(*tmp_array) * ARRAY_POP);
	if (ret < 0) {
		perror("stack_test");
		return EXIT_FAILURE;
	} else if (ret != sizeof(*tmp_array) * ARRAY_POP) {
		printf("Readed %d element instead of %d.\n",
		       ret / sizeof(*tmp_array), ARRAY_POP);
		return EXIT_FAILURE;
	}

	for (i = 0; i < ARRAY_POP; i++) {
		if (tmp_array[i] != incremental_val) {
			printf("Readed %d is false. Got %u, expected %u.\n", i,
			       tmp_array[i], incremental_val);
			return EXIT_FAILURE;
		}
		incremental_val--;
	}

	printf("Adding %d random values in one time.\n", ARRAY_RAND);
	for (i = 0; i < ARRAY_RAND; i++) {
		rand_array[i] = rand();
	}

	if (write(fd, rand_array, sizeof(uint32_t) * ARRAY_RAND) !=
	    (sizeof(uint32_t) * ARRAY_RAND)) {
		printf("Not all random data wrote.\n");
		return EXIT_FAILURE;
	}

	printf("Reading whole stack.\n");
	ret = read(fd, tmp_array, (sizeof(uint32_t) * TMP_SIZE));
	if (ret < 0) {
		perror("stack_test");
		return EXIT_FAILURE;
	}

	if (ret != (ARRAY_RAND + incremental_val + 1) * sizeof(uint32_t)) {
		printf("Not enough or too much data read. Got %d, expected %d\n",
		       ret / sizeof(uint32_t), (ARRAY_RAND + incremental_val + 1));
		return EXIT_FAILURE;
	}

	for (i = 0; i < ARRAY_RAND; i++) {
		if (tmp_array[i] != rand_array[ARRAY_RAND - i - 1]) {
			printf("Readed %d is false. Got %u, expected %u.\n", i,
			       tmp_array[i], rand_array[ARRAY_RAND - i - 1]);
			return EXIT_FAILURE;
		}
	}

	while (incremental_val > 0) {
		if (tmp_array[i] != incremental_val) {
			printf("Readed %d is false. Got %u, expected %u.\n", i,
			       tmp_array[i], incremental_val);
			return EXIT_FAILURE;
		}

		incremental_val--;
		i++;
	}

	printf("Test run successfully!\n");
	return EXIT_SUCCESS;
}
