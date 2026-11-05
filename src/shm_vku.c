#include "shm_vku.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/*
 * The shared memory itself, seen by every process: the ready flag first,
 * then the data right after it.
 *
 * @ready is volatile so that the wait loop reloads it from memory on every
 * iteration instead of keeping it in a register.
 *
 * volatile does not stop the CPU from reordering the data stores and the
 * flag store. That is fine on x86 (stores stay in order), but not
 * guaranteed on other architectures such as ARM.
 */
struct shm {
	volatile bool	ready;
	unsigned char	data[];
};

/* Total bytes to map: the ready flag plus @size bytes of data */
static size_t total_size(size_t size)
{
	return sizeof(struct shm) + size;
}

/* The shared memory, as mapped into this process */
static struct shm *get_shm(struct shm_info *info)
{
	return info->base;
}

static void clear_ready(struct shm *shm)
{
	shm->ready = false;
}

static void set_ready(struct shm *shm)
{
	shm->ready = true;
}

static void wait_until_ready(struct shm *shm)
{
	/* Sleep between checks rather than spin at 100% CPU */
	while (!shm->ready)
		usleep(100);
}

/* Map the object behind info->fd into this process */
static int map_object(struct shm_info *info, size_t size)
{
	info->base = mmap(NULL, total_size(size), PROT_READ | PROT_WRITE,
			  MAP_SHARED, info->fd, 0);
	if (info->base == MAP_FAILED)
		return -1;

	info->size = size;
	return 0;
}

/* Undo a half-finished create: close the fd and remove the object */
static void abort_create(struct shm_info *info, const char *name)
{
	close(info->fd);
	shm_unlink(name);
}

int shm_create_vku(struct shm_info *info, const char *name, size_t size)
{
	/* Step 1: create the object. O_EXCL refuses a leftover from a crash */
	info->fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0666);
	if (info->fd < 0)
		return -1;

	/* Step 2: give it a size. A new object has size 0 and would SIGBUS */
	if (ftruncate(info->fd, total_size(size)) < 0) {
		abort_create(info, name);
		return -1;
	}

	/* Step 3: map it into this process */
	if (map_object(info, size) < 0) {
		abort_create(info, name);
		return -1;
	}

	/* ftruncate zero-fills the object, so ready already starts as false */
	return 0;
}

int shm_open_vku(struct shm_info *info, const char *name, size_t size)
{
	/* Step 1: open the object that the creator made */
	info->fd = shm_open(name, O_RDWR, 0);
	if (info->fd < 0)
		return -1;

	/* Step 2: map it into this process */
	if (map_object(info, size) < 0) {
		close(info->fd);
		return -1;
	}

	return 0;
}

int shm_write_vku(struct shm_info *info, const void *buf, size_t len)
{
	struct shm *shm = get_shm(info);

	if (len > info->size) {
		errno = EINVAL;
		return -1;
	}

	/* Not ready while the data is being written */
	clear_ready(shm);
	memcpy(shm->data, buf, len);
	set_ready(shm);

	return 0;
}

int shm_read_vku(struct shm_info *info, void *buf, size_t len)
{
	struct shm *shm = get_shm(info);

	/* Never copy more than the shared memory holds */
	if (len > info->size)
		len = info->size;

	wait_until_ready(shm);
	memcpy(buf, shm->data, len);

	return 0;
}

int shm_close_vku(struct shm_info *info)
{
	if (munmap(info->base, total_size(info->size)) < 0)
		return -1;

	return close(info->fd);
}

int shm_destroy_vku(const char *name)
{
	return shm_unlink(name);
}
