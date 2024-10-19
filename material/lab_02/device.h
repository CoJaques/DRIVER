
#ifndef DEVICE_CHECKER_H
#define DEVICE_CHECKER_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Reads the first line from a file.
 * 
 * This function opens the specified file, reads the first line, and stores 
 * it in the provided buffer. The buffer is truncated to remove the newline character.
 * 
 * @param filename The path to the file to read.
 * @param buffer A pointer to the buffer where the line will be stored.
 * @param buffer_size The size of the buffer.
 * 
 * @return 0 if successful, or -1 if there is an error.
 */
int read_line_from_file(const char *filename, char *buffer, size_t buffer_size);

/**
 * @brief Checks if the device name in the file matches the expected device name.
 * 
 * This function reads the first line from the given file and compares it 
 * to the provided device name. If they do not match, an error is printed.
 * 
 * @param filename The path to the file to read.
 * @param device_name The expected device name.
 * 
 * @return 0 if the device name matches, or -1 if there is a mismatch or error.
 */
int check_device_name(char *filename, char *device_name);

/**
 * @brief Checks if the memory size in the file matches the expected size.
 * 
 * This function reads the memory size from the given file, converts it to a 
 * numeric value, and compares it to the expected size. If they do not match, 
 * an error is printed.
 * 
 * @param filename The path to the file to read.
 * @param expected_size The expected memory size.
 * 
 * @return 0 if the memory size matches, or -1 if there is a mismatch or error.
 */
int check_memory_size(char *filename, ssize_t expected_size);

#endif // DEVICE_CHECKER_H
