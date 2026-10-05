#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

static void print_error(const char *msg) {
	size_t len = 0;
	while (msg[len] != '\0')
		++len;
	write(STDERR_FILENO, msg, len);
	_exit(EXIT_FAILURE);
}

int main(void) {
	char buf[4096];
	ssize_t bytes;

	while ((bytes = read(STDIN_FILENO, buf, sizeof(buf))) != 0) {
		if (bytes < 0)
			print_error("child2: failed to read from stdin\n");

		for (ssize_t i = 0; i < bytes; ++i) {
			if (isspace(buf[i]) && buf[i] != '\n')
				buf[i] = '_';
		}

		if (write(STDOUT_FILENO, buf, bytes) != bytes)
			print_error("child2: failed to write to stdout\n");
	}

	return 0;
}