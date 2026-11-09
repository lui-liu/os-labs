#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>

#define SHM_SIZE 4096
#define STOP_MARKER UINT32_MAX

struct shared {
	uint32_t length;
	char data[SHM_SIZE - sizeof(uint32_t)];
};

static inline void print_msg(const char *msg) {
	size_t len = 0;
	while (msg[len] != '\0')
		++len;
	write(STDERR_FILENO, msg, len);
}

static inline void print_error(const char *msg) {
	print_msg(msg);
	_exit(EXIT_FAILURE);
}

static inline struct shared *map_shared(const char *name) {
	int shm = shm_open(name, O_RDWR, 0);
	if (shm == -1)
		return NULL;

	void *addr = mmap(NULL, sizeof(struct shared), PROT_READ | PROT_WRITE,
	                  MAP_SHARED, shm, 0);

	close(shm);

	return addr == MAP_FAILED ? NULL : addr;
}

static inline int sem_wait_retry(sem_t *sem) {
	int result;
	while ((result = sem_wait(sem)) == -1 && errno == EINTR)
		;
	return result;
}

#endif