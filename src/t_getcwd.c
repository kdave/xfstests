#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TIMEOUT 5
#define BUF_SIZE 256

static sig_atomic_t end;

int test_getcwd(char *init_cwd)
{
	int i = 0;
	char cur_cwd[BUF_SIZE];
	while (1) {
		getcwd(cur_cwd, BUF_SIZE);
		if (strncmp(init_cwd, cur_cwd, BUF_SIZE)) {
			printf("[%u] %s != %s\n", i, init_cwd, cur_cwd);
			return 1;
		}
		if (end)
			break;
		i++;
	}
	return 0;
}

void do_rename(char *prefix)
{
	int i = 0;
	int fd;
	char c_name[BUF_SIZE];
	char n_name[BUF_SIZE];

	strncpy(c_name, prefix, BUF_SIZE);

	fd = open(c_name, O_CREAT | O_RDWR, 0644);
	if (fd < 0) {
		fprintf(stderr, "failed to create file %s: %s\n",
			c_name, strerror(errno));
		exit(1);
	}
	close(fd);

	while (1) {
		i++;
		snprintf(n_name, BUF_SIZE, "%s%u", prefix, i);
		rename(c_name, n_name);
		strncpy(c_name, n_name, BUF_SIZE);
	}
}

void sigproc(int sig)
{
	end = 1;
}

int main(int argc, char *argv[])
{
	char *init_cwd;
	pid_t pid;
	int status;
	int ret = 1;

	if (argc != 2) {
		printf("Usage: %s <dir>\n", argv[0]);
		exit(1);
	}

	init_cwd = argv[1];
	ret = chdir(init_cwd);
	if (ret < 0) {
		perror("chdir failed");
		exit(1);
	}

	if (signal(SIGALRM, sigproc) == SIG_ERR) {
		perror("signal failed");
		exit(1);
	}

	alarm(TIMEOUT);

	pid = fork();
	if (pid < 0) {
		perror("fork failed");
		exit(1);
	} else if (pid == 0) {
		do_rename("t_getcwd_testfile");
	} else {
		ret = test_getcwd(init_cwd);
		kill(pid, SIGTERM);
		waitpid(pid, &status, 0);
	}

	exit(ret);
}
