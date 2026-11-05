/*
 * Test 3 - writer: fill the whole shared memory with one letter, again and
 * again, until you stop it with Ctrl+C.
 * Usage: writer <letter>
 *
 * Run two writers at the same time with different letters, for example
 * "./writer A" and "./writer B". Start ./reader first, it creates the
 * shared memory.
 */
#include "shm_vku.h"

#include <stdio.h>
#include <string.h>

/* Must be the same in reader.c */
#define SHM_NAME	"/vku_test3"
#define SHM_SIZE	4096

/* The 4096 bytes are shown as 16 letters, one for every 256 bytes */
#define SHOWN_LETTERS	16
#define SHOWN_STEP	(SHM_SIZE / SHOWN_LETTERS)

int main(int argc, char *argv[])
{
	struct shm_info info;
	char buffer[SHM_SIZE];
	int i;

	if (argc < 2) {
		fprintf(stderr, "usage: %s <letter>\n", argv[0]);
		return 1;
	}

	if (shm_open_vku(&info, SHM_NAME, SHM_SIZE) < 0) {
		perror("shm_open_vku");
		return 1;
	}

	memset(buffer, argv[1][0], sizeof(buffer));

	printf("[writer %c] ", argv[1][0]);
	for (i = 0; i < SHOWN_LETTERS; i++)
		putchar(buffer[i * SHOWN_STEP]);
	printf("\n");

	/* Runs until you stop it with Ctrl+C */
	while (1)
		shm_write_vku(&info, buffer, sizeof(buffer));
}
