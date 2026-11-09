#include "common.h"
#include <ctype.h>

int main(int argc, char **argv) {
	if (argc != 4)
		print_error("usage: child2 <shm> <sem_in> <sem_out>\n");

	struct shared *sh = map_shared(argv[1]);
	if (sh == NULL)
		print_error("child2: failed to map shared memory\n");

	sem_t *in = sem_open(argv[2], 0);
	if (in == SEM_FAILED)
		print_error("child2: failed to open input semaphore\n");

	sem_t *out = sem_open(argv[3], 0);
	if (out == SEM_FAILED)
		print_error("child2: failed to open output semaphore\n");

	while (1) {
		if (sem_wait_retry(in) == -1)
			print_error("child2: failed to wait for semaphore\n");

		if (sh->length == STOP_MARKER) {
			if (sem_post(out) == -1)
				print_error("child2: failed to post semaphore\n");
			break;
		}

		if (sh->length > sizeof(sh->data))
			print_error("child2: invalid length in shared memory\n");

		for (uint32_t i = 0; i < sh->length; ++i) {
			if (isspace((unsigned char)sh->data[i]) && sh->data[i] != '\n')
				sh->data[i] = '_';
		}

		if (sem_post(out) == -1)
			print_error("child2: failed to post semaphore\n");
	}

	sem_close(in);
	sem_close(out);
	munmap(sh, sizeof(*sh));

	return 0;
}