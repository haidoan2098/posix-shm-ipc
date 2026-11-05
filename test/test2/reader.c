/*
 * Test 2 - reader: start two reader processes. Each one opens the shared
 * memory by itself, waits for the writer, then reads it twice a second
 * until you stop it with Ctrl+C.
 * Usage: reader
 *
 * Run ./writer first.
 */
#include "shm_vku.h"

#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

/* Must be the same in writer.c */
#define SHM_NAME	"/vku_test2"
#define SHM_SIZE	4096
#define NUM_READERS	2

/* Body of one reader process */
static int run_reader(void)
{
	struct shm_info info;
	char buffer[SHM_SIZE];
	int pid = getpid();

	if (shm_open_vku(&info, SHM_NAME, SHM_SIZE) < 0) {
		perror("shm_open_vku");
		return 1;
	}
	printf("[reader %d] opened %s, waiting for the writer...\n", pid,
	       SHM_NAME);

	/* Runs until you stop it with Ctrl+C */
	while (1) {
		shm_read_vku(&info, buffer, sizeof(buffer));
		printf("[reader %d] read: \"%s\"\n", pid, buffer);
		usleep(500000);
	}
}

int main(void)
{
	int i;

	/* Each child is a separate process with its own PID */
	for (i = 0; i < NUM_READERS; i++) {
		if (fork() == 0)
			return run_reader();
	}

	/* The readers never finish by themselves; Ctrl+C stops the whole group */
	for (i = 0; i < NUM_READERS; i++)
		wait(NULL);

	return 0;
}
