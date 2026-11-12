/*
 * Test 1 - writer: create the shared memory, then write a new message
 * every second until you stop it with Ctrl+C.
 * Usage: writer
 *
 * Start this first, then run ./reader in another terminal.
 */
#include "shm_vku.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Must be the same in reader.c */
#define SHM_NAME	"/vku_test1"
#define SHM_SIZE	4096

int main(void)
{
	struct shm_info info;
	char message[64];
	int i;

	/* Remove a leftover from an earlier run stopped with Ctrl+C */
	shm_destroy_vku(SHM_NAME);

	if (shm_create_vku(&info, SHM_NAME, SHM_SIZE) < 0) {
		perror("shm_create_vku");
		return 1;
	}
	printf("[writer] created %s, now run ./reader in another terminal\n",
	       SHM_NAME);

	printf("[writer] press Enter to start writing...");
	getchar();

	/* Runs until you stop it with Ctrl+C */
	for (i = 1; ; i++) {
		snprintf(message, sizeof(message), "message %d", i);
		shm_write_vku(&info, message, strlen(message) + 1);
		printf("[writer] wrote: \"%s\"\n", message);
		sleep(1);
	}
}
