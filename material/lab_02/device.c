#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int read_line_from_file(const char *filename, char *buffer, size_t buffer_size)
{
	FILE *file = fopen(filename, "r");
	if (!file) {
		perror("Error opening file");
		return -1;
	}
	if (!fgets(buffer, buffer_size, file)) {
		perror("Error reading file");
		fclose(file);
		return -1;
	}
	fclose(file);

	// delete new line char
	buffer[strcspn(buffer, "\n")] = '\0';
	return 0;
}

int check_device_name(char *filename, char *device_name)
{
	char name_buffer[256];
	if (read_line_from_file(filename, name_buffer, sizeof(name_buffer)) !=
	    0) {
		return -1;
	}
	if (strcmp(name_buffer, device_name) != 0) {
		fprintf(stderr,
			"Error: Device name mismatch. Expected %s but got %s\n",
			device_name, name_buffer);
		return -1;
	}
	printf("Device name is correct: %s\n", name_buffer);
	return 0;
}

int check_memory_size(char *filename, ssize_t expected_size)
{
	char size_buffer[256];
	unsigned long mem_size;

	if (read_line_from_file(filename, size_buffer, sizeof(size_buffer)) !=
	    0) {
		return -1;
	}
	mem_size = strtoul(size_buffer, NULL, 0);
	if (mem_size != expected_size) {
		fprintf(stderr,
			"Error: Memory size mismatch. Expected %zd but got %lu\n",
			expected_size, mem_size);
		return -1;
	}
	printf("Memory size is correct: 0x%lx bytes\n", mem_size);
	return 0;
}
