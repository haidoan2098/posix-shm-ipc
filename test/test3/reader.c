/*
 * Test 3 - reader: create the shared memory, then keep reading it and
 * print what it got. Stop it with Ctrl+C.
 * Usage: reader
 *
 * Run this first, then run two writers in two other terminals.
 */
#include "shm_vku.h"

#include <stdio.h>
#include <unistd.h>

/* Must be the same in writer.c */
#define SHM_NAME	"/vku_test3"
#define SHM_SIZE	4096

/* The 4096 bytes are shown as 16 letters, one for every 256 bytes */
#define SHOWN_LETTERS	16
#define SHOWN_STEP	(SHM_SIZE / SHOWN_LETTERS)

int main(void)
{
	struct shm_info info;
	char buffer[SHM_SIZE];
	int i;

	/* Remove a leftover from an earlier run stopped with Ctrl+C */
	shm_destroy_vku(SHM_NAME);

	if (shm_create_vku(&info, SHM_NAME, SHM_SIZE) < 0) {
		perror("shm_create_vku");
		return 1;
	}
	printf("[reader] created %s, now run ./writer A and ./writer B\n",
	       SHM_NAME);

	/* Runs until you stop it with Ctrl+C */
	while (1) {
		shm_read_vku(&info, buffer, sizeof(buffer));

		printf("[reader] ");
		for (i = 0; i < SHOWN_LETTERS; i++)
			putchar(buffer[i * SHOWN_STEP]);
		printf("\n");

		usleep(500000);
	}
}
