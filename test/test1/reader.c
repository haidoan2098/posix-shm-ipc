/*
 * Test 1 - reader: open the shared memory, wait for the writer, then read
 * it twice a second until you stop it with Ctrl+C.
 * Usage: reader
 *
 * Run ./writer first.
 */
#include "shm_vku.h"

#include <stdio.h>
#include <unistd.h>

/* Must be the same in writer.c */
#define SHM_NAME	"/vku_test1"
#define SHM_SIZE	4096

int main(void)
{
	struct shm_info info;
	char buffer[SHM_SIZE];

	if (shm_open_vku(&info, SHM_NAME, SHM_SIZE) < 0) {
		perror("shm_open_vku");
		return 1;
	}
	printf("[reader] opened %s, waiting for the writer...\n", SHM_NAME);

	/* Runs until you stop it with Ctrl+C */
	while (1) {
		shm_read_vku(&info, buffer, sizeof(buffer));
		printf("[reader] read: \"%s\"\n", buffer);
		usleep(500000);
	}
}
