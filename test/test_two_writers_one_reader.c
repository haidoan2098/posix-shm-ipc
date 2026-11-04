/*
 * Test 2: two writers, one reader.
 * Writer A fills the whole buffer with 'A', writer B with 'B'. If the lock
 * works, the reader never sees both letters in one read.
 */
#include "shm_vku.h"

#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define TEST_NAME	"/vku_test_2w1r"
#define TEST_SIZE	4096
#define NUM_WRITES	500

/* Set by -v: print every buffer the reader gets */
static int verbose;

static void writer(char letter)
{
	char buf[TEST_SIZE];
	struct shm_vku w;
	int i;

	memset(buf, letter, sizeof(buf));
	shm_open_vku(&w, TEST_NAME, TEST_SIZE);
	for (i = 0; i < NUM_WRITES; i++)
		shm_write_vku(&w, buf, sizeof(buf));
	shm_close_vku(&w);
}

static int reader(void)
{
	unsigned long version = 0;
	char buf[TEST_SIZE];
	struct shm_vku r;
	int i;

	shm_open_vku(&r, TEST_NAME, TEST_SIZE);
	while (version < 2 * NUM_WRITES) {
		shm_read_vku(&r, buf, sizeof(buf), &version);
		if (verbose)
			printf("[reader] version %lu: '%c' x %d\n",
			       version, buf[0], TEST_SIZE);
		for (i = 1; i < TEST_SIZE; i++) {
			if (buf[i] != buf[0])
				return 1;
		}
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
		writer('A');
		_exit(0);
	}
	if (fork() == 0) {
		writer('B');
		_exit(0);
	}

	for (i = 0; i < 3; i++) {
		wait(&status);
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
			ok = 0;
	}

	shm_close_vku(&shm);
	shm_destroy_vku(TEST_NAME);

	if (!ok) {
		printf("[FAIL] 2 writers, 1 reader\n");
		return 1;
	}

	printf("[PASS] 2 writers, 1 reader\n");
	return 0;
}
