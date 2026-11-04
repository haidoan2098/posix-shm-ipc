/*
 * Thin wrapper around POSIX shared memory.
 *
 * Segment layout:
 *
 *	+----------------------+  <- mmap base
 *	| header               |  mutex, condvar, version
 *	+----------------------+
 *	| data                 |  size bytes
 *	+----------------------+
 *
 * All access to the data goes through shm_write_vku() / shm_read_vku(),
 * which take the lock internally. Callers never touch the lock.
 *
 * Functions return -1 on error and set errno.
 */
#ifndef SHM_VKU_H
#define SHM_VKU_H

#include <stddef.h>
#include <sys/types.h>

struct shm_vku {
	int	fd;
	void	*base;	/* start of the mapping (header + data) */
	size_t	size;	/* data capacity, header not included */
};

/* Create a new segment. Fails with EEXIST if @name already exists. */
int shm_create_vku(struct shm_vku *shm, const char *name, size_t size);

/* Open a segment created by shm_create_vku() with the same @size. */
int shm_open_vku(struct shm_vku *shm, const char *name, size_t size);

/*
 * Copy @len bytes from @buf into the segment and wake up all readers.
 * Fails with EINVAL if @len is larger than the segment.
 */
int shm_write_vku(struct shm_vku *shm, const void *buf, size_t len);

/*
 * Block until the segment holds data newer than *@version, then copy
 * @len bytes (at most the segment size) into @buf and update *@version.
 *
 * Start with *@version = 0 to wait for the first write. Keep passing the
 * same variable to receive each new update.
 *
 * This always returns the latest data, it is not a queue: if the writer
 * updates several times before the reader calls again, the versions in
 * between are skipped.
 *
 * Returns the number of bytes copied.
 */
ssize_t shm_read_vku(struct shm_vku *shm, void *buf, size_t len,
		     unsigned long *version);

/* Unmap and close. The object itself stays in /dev/shm. */
int shm_close_vku(struct shm_vku *shm);

/* Remove the object. Existing mappings stay valid until unmapped. */
int shm_destroy_vku(const char *name);

#endif /* SHM_VKU_H */
