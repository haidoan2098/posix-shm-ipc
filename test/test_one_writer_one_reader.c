/*
 * Test 1: one writer, one reader.
 * The writer keeps writing numbered messages, the reader keeps reading.
 * Every message the reader gets must be complete, and the numbers must
 * keep increasing (some may be skipped, since a read returns the latest).
 */
#include "shm_vku.h"

#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define TEST_NAME	"/vku_test_1w1r"
#define TEST_SIZE	4096
#define NUM_WRITES	500

/* Set by -v: print every message the reader gets */
static int verbose;

static void writer(void)
{
	struct shm_vku w;
	char buf[64];
	int i;

	shm_open_vku(&w, TEST_NAME, TEST_SIZE);
	for (i = 1; i <= NUM_WRITES; i++) {
		snprintf(buf, sizeof(buf), "hello from writer %d", i);
		shm_write_vku(&w, buf, sizeof(buf));
	}
	shm_close_vku(&w);
}

static int reader(void)
{
	unsigned long version = 0;
	struct shm_vku r;
	char buf[64];
	int last = 0;
	int n;

	shm_open_vku(&r, TEST_NAME, TEST_SIZE);
	while (last < NUM_WRITES) {
		shm_read_vku(&r, buf, sizeof(buf), &version);
		if (verbose)
			printf("[reader] version %lu: %s\n", version, buf);
		if (sscanf(buf, "hello from writer %d", &n) != 1 || n <= last)
			return 1;
		last = n;
	}
	shm_close_vku(&r);
	return 0;
}

int main(int argc, char *argv[])
{
	struct shm_vku shm;
	int status;
	int ok = 1;
	int i;

	if (argc > 1 && strcmp(argv[1], "-v") == 0)
		verbose = 1;

	/* Remove a leftover object from an earlier crashed run */
	shm_destroy_vku(TEST_NAME);
	shm_create_vku(&shm, TEST_NAME, TEST_SIZE);

	if (fork() == 0) {
		int ret = reader();

		/* _exit() does not flush stdio, so flush the reader's output */
		fflush(stdout);
		_exit(ret);
	}
	if (fork() == 0) {
		writer();
		_exit(0);
	}

	for (i = 0; i < 2; i++) {
		wait(&status);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
			ok = 0;
	}

	shm_close_vku(&shm);
	shm_destroy_vku(TEST_NAME);

	if (!ok) {
		printf("[FAIL] 1 writer, 1 reader\n");
		return 1;
	}

	printf("[PASS] 1 writer, 1 reader\n");
	return 0;
}
