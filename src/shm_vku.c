#include "shm_vku.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/*
 * Lives at the start of the segment, shared by every process. The data
 * follows right after the header fields.
 *
 * @lock and @updated must be created with PTHREAD_PROCESS_SHARED, since a
 * normal pthread mutex only works between threads of one process.
 */
struct shm_vku_header {
	pthread_mutex_t	lock;
	pthread_cond_t	updated;
	unsigned long	version;	/* incremented on every write */
	unsigned char	data[];
};

static struct shm_vku_header *get_header(struct shm_vku *shm)
{
	return shm->base;
}

static int map_segment(struct shm_vku *shm, size_t size)
{
	size_t map_size = sizeof(struct shm_vku_header) + size;

	shm->base = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_SHARED,
			 shm->fd, 0);
	if (shm->base == MAP_FAILED)
		return -1;

	shm->size = size;
	return 0;
}

static void init_header(struct shm_vku_header *hdr)
{
	pthread_mutexattr_t mattr;
	pthread_condattr_t cattr;

	pthread_mutexattr_init(&mattr);
	pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
	/* Without this, a process dying while holding the lock hangs everyone */
	pthread_mutexattr_setrobust(&mattr, PTHREAD_MUTEX_ROBUST);
	pthread_mutex_init(&hdr->lock, &mattr);
	pthread_mutexattr_destroy(&mattr);

	pthread_condattr_init(&cattr);
	pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
	pthread_cond_init(&hdr->updated, &cattr);
	pthread_condattr_destroy(&cattr);

	/* ftruncate zero-fills, so version already starts at 0 */
}

/*
 * EOWNERDEAD means the previous owner died while holding the lock. We still
 * get the lock, but must mark it consistent before it can be used again.
 * The data may be left half-written by the dead process.
 */
static int recover_lock(struct shm_vku_header *hdr, int ret)
{
	if (ret == EOWNERDEAD)
		ret = pthread_mutex_consistent(&hdr->lock);
	return ret;
}

int shm_create_vku(struct shm_vku *shm, const char *name, size_t size)
{
	/* O_EXCL: never silently reuse a stale object left by a crash */
	shm->fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0666);
	if (shm->fd < 0)
		return -1;

	/* A new object has size 0; touching the mapping would SIGBUS */
	if (ftruncate(shm->fd, sizeof(struct shm_vku_header) + size) < 0) {
		close(shm->fd);
		shm_unlink(name);
		return -1;
	}

	if (map_segment(shm, size) < 0) {
		close(shm->fd);
		shm_unlink(name);
		return -1;
	}

	init_header(get_header(shm));
	return 0;
}

int shm_open_vku(struct shm_vku *shm, const char *name, size_t size)
{
	shm->fd = shm_open(name, O_RDWR, 0);
	if (shm->fd < 0)
		return -1;

	if (map_segment(shm, size) < 0) {
		close(shm->fd);
		return -1;
	}

	return 0;
}

int shm_write_vku(struct shm_vku *shm, const void *buf, size_t len)
{
	struct shm_vku_header *hdr = get_header(shm);
	int ret;

	if (len > shm->size) {
		errno = EINVAL;
		return -1;
	}

	ret = recover_lock(hdr, pthread_mutex_lock(&hdr->lock));
	if (ret != 0) {
		errno = ret;
		return -1;
	}

	memcpy(hdr->data, buf, len);
	hdr->version++;

	pthread_cond_broadcast(&hdr->updated);
	pthread_mutex_unlock(&hdr->lock);
	return 0;
}

ssize_t shm_read_vku(struct shm_vku *shm, void *buf, size_t len,
		     unsigned long *version)
{
	struct shm_vku_header *hdr = get_header(shm);
	int ret;

	if (len > shm->size)
		len = shm->size;

	ret = recover_lock(hdr, pthread_mutex_lock(&hdr->lock));
	if (ret != 0) {
		errno = ret;
		return -1;
	}

	/* Loop: cond_wait may wake up without a new write (spurious wakeup) */
	while (hdr->version == *version) {
		ret = recover_lock(hdr, pthread_cond_wait(&hdr->updated,
							  &hdr->lock));
		if (ret != 0) {
			pthread_mutex_unlock(&hdr->lock);
			errno = ret;
			return -1;
		}
	}

	memcpy(buf, hdr->data, len);
	*version = hdr->version;

	pthread_mutex_unlock(&hdr->lock);
	return len;
}

int shm_close_vku(struct shm_vku *shm)
{
	if (munmap(shm->base, sizeof(struct shm_vku_header) + shm->size) < 0)
		return -1;

	return close(shm->fd);
}

int shm_destroy_vku(const char *name)
{
	return shm_unlink(name);
}
