/*
 * Test program used to validate the functional behavior of our driver.
 * v1.1, 22.07.2022
 *
 * We hereby test that:
 * - writing a sequence of integers corresponding to our string results in the
 *   expected integers being returned when reading from the device
 * - after the encrypted vector is retrieved, another vector can be successfully
 *   encrypted
 * - the size of the vector read can be smaller than the size of the written
 *   vector
 * - attempting to write twice in the device (without read()s in between) will
 *   result in an error
 * - reading from an empty buffer will not return a vector created from thin air
 * - trying to write a vector whose size is not a multiple of the size of an
 *   integer will result in an error
 * - trying to read a vector whose size is not a multiple of the size of an
 *   integer will result in nothing read
 *
 * Note: in an industrial setting, a proper test framework should be used !
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

/* Hardcoded path to our device file. */
#define DEV_PATH "/dev/reds-adder"

/* Size of the buffer used for read()s.*/
#define BUF_SIZE 30

/* Threshold that has been used in the encryption process. */
#define THR	 0x03

int main()
{
	int fd;
	/* First message, easy to check. */
	char const *msg1_char = "AAAAAAAAAAAA";
	int msg1[BUF_SIZE];

	/*
	  Second message, shorter so that spurious characters are easy to spot.
	 */
	char const *msg2_char = "BBBBB";
	int msg2[BUF_SIZE];
	/* Destination buffer for our read() operations. */
	int buf[BUF_SIZE + 1];
	int rc;
	int i;

	/*
	  Convert our message from text to integers, since our device operates on
	  integers only and we have a binary interface to it.
	*/
	for (i = 0; i < BUF_SIZE; ++i) {
		msg1[i] = 0;
		msg2[i] = 0;
	}
	for (i = 0; i < (int)strlen(msg1_char); ++i) {
		msg1[i] = (int)msg1_char[i];
	}
	for (i = 0; i < (int)strlen(msg2_char); ++i) {
		msg2[i] = (int)msg2_char[i];
	}

	/* Open our device file. */
	fd = open(DEV_PATH, O_RDWR);
	if (fd == -1) {
		perror("open");
		return 1;
	}

	/* Write the first message. */
	rc = write(fd, msg1, strlen(msg1_char) * sizeof(int));
	assert(rc == (int)strlen(msg1_char) * sizeof(int));

	/*
	  Read the encrypted message (in integer form) and check that it matches
	  our expectations.
	*/
	rc = read(fd, buf, BUF_SIZE * sizeof(int));
	assert(rc == (int)strlen(msg1_char) * sizeof(int));
	{
		int incr = 1;
		for (i = 0; i < (int)strlen(msg1_char); ++i, ++incr) {
			assert(buf[i] == msg1[i] + incr);
			if (incr == THR) {
				incr = 0;
			}
		}
	}

	/* Write the second message. */
	rc = write(fd, msg2, strlen(msg2_char) * sizeof(int));
	assert(rc == (int)strlen(msg2_char) * sizeof(int));

	/*
	  Read the encrypted message (in integer form) and check that it matches
	  our expectations.
	*/
	rc = read(fd, buf, BUF_SIZE * sizeof(int));
	assert(rc == (int)strlen(msg2_char) * sizeof(int));
	{
		int incr = 1;
		for (i = 0; i < (int)strlen(msg2_char); ++i, ++incr) {
			assert(buf[i] == msg2[i] + incr);
			if (incr == THR) {
				incr = 0;
			}
		}
	}

	/* Now check multiple read()s and multiple write()s. */
	rc = write(fd, msg1, strlen(msg1_char) * sizeof(int));
	assert(rc == (int)strlen(msg1_char) * sizeof(int));
	fprintf(stderr, "\n!!! Expect some error messages below !!!\n");
	rc = write(fd, msg1, strlen(msg1_char) * sizeof(int));
	assert(rc == -1);
	rc = read(fd, buf, BUF_SIZE * sizeof(int));
	assert(rc == (int)strlen(msg1_char) * sizeof(int));
	rc = read(fd, buf, BUF_SIZE * sizeof(int));
	assert(rc == 0);

	/* Verify that the device operates on integers only. */
	rc = write(fd, msg1, strlen(msg1_char) * sizeof(int) + 1);
	assert(rc == -1);
	rc = write(fd, msg1, strlen(msg1_char) * sizeof(int));
	assert(rc == (int)strlen(msg1_char) * sizeof(int));
	rc = read(fd, buf, BUF_SIZE * sizeof(int) - 1);
	assert(rc == 0);
	rc = read(fd, buf, BUF_SIZE * sizeof(int));
	assert(rc == (int)strlen(msg1_char) * sizeof(int));

	/* Write the second message again. */
	rc = write(fd, msg2, strlen(msg2_char) * sizeof(int));
	assert(rc == (int)strlen(msg2_char) * sizeof(int));

	/*
	  Read the encrypted message (in integer form) but skipping the last
	  integer value (corresponding to the last character), and check that it
	  matches our expectations.
	*/
	rc = read(fd, buf, ((int)strlen(msg2_char) - 1) * sizeof(int));
	assert(rc == ((int)strlen(msg2_char) - 1) * sizeof(int));
	{
		int incr = 1;
		for (i = 0; i < (int)strlen(msg2_char) - 1; ++i, ++incr) {
			assert(buf[i] == msg2[i] + incr);
			if (incr == THR) {
				incr = 0;
			}
		}
	}

	close(fd);

	fprintf(stderr, "\nAll checks are OK !\n");

	return 0;
}
