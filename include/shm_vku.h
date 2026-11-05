/*
 * Thin wrapper around POSIX shared memory.
 *
 * Shared memory layout:
 *
 *	+----------------------+  <- mmap base
 *	| ready flag           |
 *	+----------------------+
 *	| data                 |  size bytes
 *	+----------------------+
 *
 * Synchronization is a single ready flag: the writer sets it after the
 * data is written, readers wait for it before reading. This is safe for
 * ONE writer that writes once. There is no lock, so rewriting while a
 * reader is reading, or using several writers, can give torn data.
 *
 * Functions return 0 on success, -1 on error and set errno.
 */
#ifndef SHM_VKU_H
#define SHM_VKU_H

#include <stddef.h>

/*
 * What this process remembers about a shared memory it has created or
 * opened. It lives in the process, not in the shared memory: every
 * process has its own copy, with its own fd and base address.
 */
struct shm_info {
	int	fd;	/* file descriptor from shm_open() */
	void	*base;	/* address returned by mmap(): start of the shared memory */
	size_t	size;	/* data capacity in bytes, the ready flag not included */
};

/* Create a new shared memory. Fails with EEXIST if @name already exists. */
int shm_create_vku(struct shm_info *info, const char *name, size_t size);

/* Open a shared memory created by shm_create_vku() with the same @size. */
int shm_open_vku(struct shm_info *info, const char *name, size_t size);

/*
 * Copy @len bytes from @buf into the shared memory, then set the ready
 * flag. Fails with EINVAL if @len is larger than the data capacity.
 */
int shm_write_vku(struct shm_info *info, const void *buf, size_t len);

/*
 * Wait until the ready flag is set, then copy @len bytes (at most the
 * data capacity) into @buf. Waits forever if nobody writes.
 */
int shm_read_vku(struct shm_info *info, void *buf, size_t len);

/* Unmap and close. The object itself stays in /dev/shm. */
int shm_close_vku(struct shm_info *info);

/* Remove the object. Existing mappings stay valid until unmapped. */
int shm_destroy_vku(const char *name);

#endif /* SHM_VKU_H */
