
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

struct music_data {
	uint16_t duration;
	char title[25];
	char artist[25];
};

void usage(const char *prog_name)
{
	fprintf(stderr, "Usage: %s <duration> <title> <artist>\n", prog_name);
	fprintf(stderr,
		"  duration: duration of the song in seconds (integer)\n");
	fprintf(stderr, "  title: title of the song (max 24 characters)\n");
	fprintf(stderr, "  artist: name of the artist (max 24 characters)\n");
}

int main(int argc, char *argv[])
{
	struct music_data music;
	int fd;
	ssize_t written;

	if (argc != 4) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	music.duration = atoi(argv[1]);
	if (music.duration <= 0) {
		fprintf(stderr,
			"Error: duration must be a positive integer.\n");
		return EXIT_FAILURE;
	}

	strncpy(music.title, argv[2], sizeof(music.title) - 1);
	music.title[sizeof(music.title) - 1] = '\0'; // Ensure null-termination

	if (strlen(music.title) == 0) {
		fprintf(stderr, "Error: title cannot be empty.\n");
		return EXIT_FAILURE;
	}

	strncpy(music.artist, argv[3], sizeof(music.artist) - 1);
	music.artist[sizeof(music.artist) - 1] =
		'\0'; // Ensure null-termination

	if (strlen(music.artist) == 0) {
		fprintf(stderr, "Error: artist name cannot be empty.\n");
		return EXIT_FAILURE;
	}

	fd = open("/dev/drivify", O_WRONLY);
	if (fd < 0) {
		perror("Failed to open /dev/drivify");
		return EXIT_FAILURE;
	}

	written = write(fd, &music, sizeof(music));
	if (written < 0) {
		perror("Failed to write to /dev/drivify");
		close(fd);
		return EXIT_FAILURE;
	} else if (written != sizeof(music)) {
		fprintf(stderr, "Partial write: only %zd bytes written.\n",
			written);
		close(fd);
		return EXIT_FAILURE;
	}

	printf("Added song: '%s' by '%s', duration: %u seconds.\n", music.title,
	       music.artist, music.duration);

	close(fd);
	return EXIT_SUCCESS;
}
