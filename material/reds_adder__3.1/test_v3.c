/*
 * Test program used to validate the functional behavior of our driver.
 * v3.1, 22.07.2020
 *
 * A first write is performed to check that the device performs as expected.
 * The write is then performed again, and a read of twice the size is executed.
 * This is expected to block until another write, performed in a thread, unblocks
 * the read and makes the test end.
 *
 * Note: in an industrial setting, a proper test framework should be used !
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

/* Hardcoded path to our device file. */
#define DEV_PATH	"/dev/reds-adder0"

/* Size of the buffer used for read()s.*/
#define BUF_SIZE	30

/* Threshold that has been used in the encryption process. */
#define THR		0x03

/* Sleep time before doing the second write. */
#define SLEEP_TIME	2

/* Message to encrypt, in char format. */
char const *msg_char = "AAAAAAAAAAAA";

/* Message to decrypt. */
int const msg_to_decrypt[] = {11, 12, 13, 14, 15, 16, 17};

/* Message decrypted. */
int const msg_decrypted[] = {10, 10, 10, 13, 13, 13, 16};


/* Structure used to pass data to the thread. */
struct data {
	int fd;
	int msg[BUF_SIZE];
	int len;
};

void *writer_2(void *param)
{
	struct data *data = (struct data *)param;
	int rc;
	sleep(SLEEP_TIME);
	rc = write(data->fd, data->msg, data->len*sizeof(int));
	assert (rc == data->len*sizeof(int));
	return NULL;
}

int main()
{
	int rc;
	int i;
	struct data data;
	FILE *fp;
	/*
	  Writer thread, that will sleep and then perform the additional write.
	*/
	pthread_t writer_thread;
	/* Destination buffer for our read() operations. */
	int buf[BUF_SIZE+1];

	data.len = strlen(msg_char);

	/*
	  Convert our message from text to integers, since our device operates on
	  integers only and we have a binary interface to it.
	*/
	for (i = 0; i < BUF_SIZE; ++i) {
		data.msg[i] = 0;
	}
	for (i = 0; i < data.len; ++i) {
		data.msg[i] = (int)msg_char[i];
	}

	/* Open our device file. */
	data.fd = open(DEV_PATH, O_RDWR);
	if (data.fd == -1) {
		perror ("open");
		return 1;
	}

	/* Write the first message. */
	rc = write(data.fd, data.msg, data.len*sizeof(int));
	assert (rc == data.len*sizeof(int));

	/* Read exactly the number of values we expect. This should not block. */
	rc = read(data.fd, buf, data.len*sizeof(int));
	assert (rc == data.len*sizeof(int));
	{
		int incr = 1;
		for (i = 0; i < data.len; ++i, ++incr) {
			assert (buf[i] == data.msg[i]+incr);
			if (incr == THR) {
				incr = 0;
			}
		}
	}

	/* Write again the first message. */
	rc = write(data.fd, data.msg, data.len*sizeof(int));
	assert (rc == data.len*sizeof(int));

	/*
	  Create a second writer that will trig 3 seconds later, to unblock the
	  read.
	*/
	if(pthread_create(&writer_thread, NULL, writer_2, &data)) {
		fprintf(stderr, "Error creating thread\n");
		return -1;
	}

	/* Read more values that those in the buffer. This should block. */
	rc = read(data.fd, buf, 2*data.len*sizeof(int));
	assert (rc == 2*data.len*sizeof(int));
	{
		int incr = 1;
		for (i = 0; i < data.len; ++i, ++incr) {
			assert (buf[i] == data.msg[i]+incr);
			if (incr == THR) {
				incr = 0;
			}
		}
		for (i = 0; i < data.len; ++i, ++incr) {
			assert (buf[i+data.len] == data.msg[i]+incr);
			if (incr == THR) {
				incr = 0;
			}
		}
	}

	/* Wait for the writer thread to finish. */
	if(pthread_join(writer_thread, NULL)) {
		fprintf(stderr, "join() error !\n");
		return -2;
	}

	/* Now switch to decrypt and test this functionality. */
	fp = fopen("/sys/devices/platform/ff205000.reds-adder/ra_sysfs/operation", "wt");
	if (fp == NULL) {
		fprintf(stderr, "Error opening sysfs file!\n");
		return -3;
	}
	fprintf(fp, "decrypt");
	fclose(fp);

	/* Write the message to decrypt. */
	rc = write(data.fd, msg_to_decrypt, sizeof(msg_to_decrypt));
	assert (rc == sizeof(msg_to_decrypt));

	/* Read exactly the number of values we expect. This should not block. */
	rc = read(data.fd, buf, sizeof(msg_to_decrypt));
	assert (rc == sizeof(msg_to_decrypt));
	{
		int incr = 1;
		for (i = 0; i < sizeof(msg_to_decrypt)/sizeof(int); ++i, ++incr) {
			assert (buf[i] == msg_decrypted[i]);
			if (incr == THR) {
				incr = 0;
			}
		}
	}

	fprintf(stderr, "\nAll checks are OK !\n");

	return 0;
}
