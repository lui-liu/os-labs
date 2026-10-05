#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

static char PROGRAM_CHILD_1[] = "child1";
static char PROGRAM_CHILD_2[] = "child2";

static void print_error(const char *msg) {
	size_t len = 0;
	while (msg[len] != '\0')
		++len;
	write(STDERR_FILENO, msg, len);
	_exit(EXIT_FAILURE);
}

int main(void) {
	
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		print_error("parent: failed to set SIGPIPE handler\n");

	int pipe1[2]; 
	if (pipe(pipe1) == -1)
		print_error("parent: failed to create pipe1\n");

	int pipe3[2]; 
	if (pipe(pipe3) == -1)
		print_error("parent: failed to create pipe3\n");

	int pipe2[2]; 
	if (pipe(pipe2) == -1)
		print_error("parent: failed to create pipe2\n");

	const pid_t child1 = fork();
	if (child1 == -1)
		print_error("parent: failed to spawn child1\n");

	if (child1 == 0) {
		if (dup2(pipe1[0], STDIN_FILENO) == -1 || dup2(pipe3[1], STDOUT_FILENO) == -1)
			print_error("parent: dup2 failed for child1\n");

		close(pipe1[0]); close(pipe1[1]);
		close(pipe2[0]); close(pipe2[1]);
		close(pipe3[0]); close(pipe3[1]);

		char *const args[] = {PROGRAM_CHILD_1, NULL};
		execv("./child1", args);
		print_error("parent: failed to exec child1\n");
	}

	const pid_t child2 = fork();
	if (child2 == -1)
		print_error("parent: failed to spawn child2\n");

	if (child2 == 0) { 
		if (dup2(pipe3[0], STDIN_FILENO) == -1 || dup2(pipe2[1], STDOUT_FILENO) == -1)
			print_error("parent: dup2 failed for child2\n");

		close(pipe1[0]); close(pipe1[1]);
		close(pipe2[0]); close(pipe2[1]);
		close(pipe3[0]); close(pipe3[1]);

		char *const args[] = {PROGRAM_CHILD_2, NULL};
		execv("./child2", args);
		print_error("parent: failed to exec child2\n");
	}

	close(pipe1[0]);
	close(pipe2[1]);
	close(pipe3[0]);
	close(pipe3[1]);

	char buf[4096];
	ssize_t bytes;

	while ((bytes = read(STDIN_FILENO, buf, sizeof(buf))) != 0) {
		if (bytes < 0)
			print_error("parent: failed to read from stdin\n");

		if (buf[0] == '\n')
			break;

		if (write(pipe1[1], buf, bytes) != bytes)
			print_error("parent: failed to write to child1\n");

		ssize_t total = 0;
		while (total < bytes) {
			ssize_t got = read(pipe2[0], buf + total, bytes - total);
			if (got <= 0)
				print_error("parent: failed to read from child2\n");
			total += got;
		}

		if (write(STDOUT_FILENO, buf, total) != total)
			print_error("parent: failed to write to stdout\n");
	}

	close(pipe1[1]);
	close(pipe2[0]);

	wait(NULL);
	wait(NULL);

	return 0;
}