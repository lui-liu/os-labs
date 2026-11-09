#include "common.h"
#include <stdbool.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>

static char shm_name[64];
static char sem1_name[64];
static char sem2_name[64];
static char sem3_name[64];

static struct shared *sh = NULL;
static bool shm_created = false;
static sem_t *sem1 = SEM_FAILED;
static sem_t *sem2 = SEM_FAILED;
static sem_t *sem3 = SEM_FAILED;

static pid_t child1 = -1;
static pid_t child2 = -1;

static void release_resources(void) {
	if (sem1 != SEM_FAILED) {
		sem_close(sem1);
		sem_unlink(sem1_name);
	}
	if (sem2 != SEM_FAILED) {
		sem_close(sem2);
		sem_unlink(sem2_name);
	}
	if (sem3 != SEM_FAILED) {
		sem_close(sem3);
		sem_unlink(sem3_name);
	}
	if (sh != NULL)
		munmap(sh, sizeof(*sh));
	if (shm_created)
		shm_unlink(shm_name);
}

static pid_t wait_child(pid_t pid, int *status, int options) {
	pid_t result;
	do {
		result = waitpid(pid, status, options);
	} while (result == -1 && errno == EINTR);
	return result;
}

static void fail(const char *msg) {
	print_msg(msg);

	if (child1 > 0) {
		kill(child1, SIGKILL);
		wait_child(child1, NULL, 0);
	}
	if (child2 > 0) {
		kill(child2, SIGKILL);
		wait_child(child2, NULL, 0);
	}

	release_resources();
	_exit(EXIT_FAILURE);
}

static sem_t *create_sem(const char *name) {
	sem_t *sem = sem_open(name, O_CREAT | O_EXCL, 0600, 0);
	if (sem == SEM_FAILED)
		fail("parent: failed to create semaphore\n");
	return sem;
}

static void check_child(pid_t *pid, int options, const char *error) {
	int status;
	pid_t result = wait_child(*pid, &status, options);
	if (result == -1)
		fail("parent: failed to wait for child\n");
	if (result == 0)
		return;

	*pid = -1;
	if (options == WNOHANG || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
		fail(error);
}

static void wait_for_result(void) {
	while (1) {
		struct timespec deadline;
		if (clock_gettime(CLOCK_REALTIME, &deadline) == -1)
			fail("parent: failed to get time\n");
		deadline.tv_sec += 1;

		if (sem_timedwait(sem3, &deadline) == 0)
			return;

		if (errno == EINTR)
			continue;
		if (errno != ETIMEDOUT)
			fail("parent: failed to wait for semaphore\n");

		check_child(&child1, WNOHANG, "parent: child1 terminated unexpectedly\n");
		check_child(&child2, WNOHANG, "parent: child2 terminated unexpectedly\n");
	}
}

static void write_all(const char *data, size_t length) {
	while (length > 0) {
		ssize_t written = write(STDOUT_FILENO, data, length);
		if (written < 0 && errno == EINTR)
			continue;
		if (written <= 0)
			fail("parent: failed to write to stdout\n");
		data += written;
		length -= (size_t)written;
	}
}

int main(void) {
	struct sigaction action = { .sa_handler = SIG_IGN };
	if (sigemptyset(&action.sa_mask) == -1 ||
	    sigaction(SIGPIPE, &action, NULL) == -1)
		fail("parent: failed to ignore SIGPIPE\n");

	int id = (int)getpid();
	snprintf(shm_name, sizeof(shm_name), "/lab3-shm-%d", id);
	snprintf(sem1_name, sizeof(sem1_name), "/lab3-sem1-%d", id);
	snprintf(sem2_name, sizeof(sem2_name), "/lab3-sem2-%d", id);
	snprintf(sem3_name, sizeof(sem3_name), "/lab3-sem3-%d", id);

	int shm = shm_open(shm_name, O_RDWR | O_CREAT | O_EXCL, 0600);
	if (shm == -1)
		fail("parent: failed to create shared memory\n");
	shm_created = true;

	if (ftruncate(shm, sizeof(struct shared)) == -1) {
		close(shm);
		fail("parent: failed to resize shared memory\n");
	}

	void *addr = mmap(NULL, sizeof(struct shared), PROT_READ | PROT_WRITE,
	                  MAP_SHARED, shm, 0);
	close(shm);
	if (addr == MAP_FAILED)
		fail("parent: failed to map shared memory\n");
	sh = addr;

	sem1 = create_sem(sem1_name);
	sem2 = create_sem(sem2_name);
	sem3 = create_sem(sem3_name);

	child1 = fork();
	if (child1 == -1)
		fail("parent: failed to spawn child1\n");

	if (child1 == 0) {
		char *const args[] = {"child1", shm_name, sem1_name, sem2_name, NULL};
		execv("./child1", args);
		print_error("parent: failed to exec child1\n");
	}

	child2 = fork();
	if (child2 == -1)
		fail("parent: failed to spawn child2\n");

	if (child2 == 0) {
		char *const args[] = {"child2", shm_name, sem2_name, sem3_name, NULL};
		execv("./child2", args);
		print_error("parent: failed to exec child2\n");
	}

	ssize_t bytes;
	while ((bytes = read(STDIN_FILENO, sh->data, sizeof(sh->data))) != 0) {
		if (bytes < 0 && errno == EINTR)
			continue;
		if (bytes < 0)
			fail("parent: failed to read from stdin\n");

		sh->length = (uint32_t)bytes;

		if (sem_post(sem1) == -1)
			fail("parent: failed to post semaphore\n");
		wait_for_result();
		write_all(sh->data, (size_t)bytes);
	}

	sh->length = STOP_MARKER;
	if (sem_post(sem1) == -1)
		fail("parent: failed to post semaphore\n");
	check_child(&child1, 0, "parent: child1 terminated unexpectedly\n");
	check_child(&child2, 0, "parent: child2 terminated unexpectedly\n");
	if (sem_trywait(sem3) == -1)
		fail("parent: missing EOF acknowledgement\n");

	release_resources();
	return 0;
}
