#include <fcntl.h>
#include <unistd.h>

#include <sys/wait.h>

#include <string.h>
#include <stdio.h>

#define PROC_ACTIVE_PLUGIN "/proc/litmus/active_plugin"

/* auto generated by Makefile */
#include "../test_catalog.inc"

#include "litmus.h"

int verbose = 0;

int run_test(struct testcase *tc) {
	int status;
	pid_t pid;

	printf("** Testing: %s... ", tc->description);
	fflush(stdout);
	SYSCALL( pid = fork() );
	if (pid == 0) {
		/* child: init liblitmus and carry out test */
		SYSCALL( init_litmus() );
		tc->function();
		exit(0);
	} else {
		if (verbose) {
			printf("[PID=%d] ", pid);
			fflush(stdout);
		}
		/* parent: wait for completion of test */
		SYSCALL( waitpid(pid, &status, 0) );
		if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
			printf("ok.\n");
		else if (WIFSIGNALED(status)) {
			printf("failed (%s)!\n", strsignal(WTERMSIG(status)));
		}
	}
	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int run_tests(int* testidx, int num_tests, const char* plugin)
{
	int idx, i;
	int ok = 0;

	printf("** Running tests for %s.\n", plugin);
	for (i = 0; i < num_tests; i++) {
		idx = testidx[i];
		ok += run_test(test_catalog + idx);
	}
	return ok;
}

static int get_active_plugin(char *buf, size_t buf_size)
{
	FILE *f;
	size_t nread;

	f = fopen(PROC_ACTIVE_PLUGIN, "r");
	if (f) {
		/* proc files read in one go */
		nread = fread(buf, sizeof(char), buf_size - 1, f);
		fclose(f);

		/* remove trailing newline */
		if (nread > 0 && buf[nread - 1] == '\n')
			nread--;

		/* null terminate buffer */
		if (nread > 0) {
			buf[nread] = '\0';
			return 1;
		}
	}
	return 0;
}

const char *usage_msg =
	"Usage: runtests OPTIONS [plugin name]\n"
	"    -v                verbose (prints PIDs)\n"
	"\n";

void usage(char *error) {
	int i;
	fprintf(stderr, "%s\n%s", error, usage_msg);
	fprintf(stderr, "Supported plugins: ");
	for (i = 0; i < NUM_PLUGINS; i++)
		fprintf(stderr, "%s ", testsuite[i].plugin);
	fprintf(stderr, "\n");
	exit(1);
}

#define streq(s1, s2) (!strcmp(s1, s2))

#define OPTSTR "v"

int main(int argc, char** argv)
{
	int ok, i, opt;
	char active_plugin[256];
	char *plugin_name = NULL;

	while ((opt = getopt(argc, argv, OPTSTR)) != -1) {
		switch (opt) {
		case 'v':
			verbose = 1;
			break;
		case ':':
			usage("Argument missing.");
			break;
		case '?':
		default:
			usage("Bad argument.");
			break;
		}
	}

	argc -= optind;
	argv += optind;

	printf("** LITMUS^RT test suite.\n");

	if (argc == 1)
		plugin_name = argv[0];
	else if (get_active_plugin(active_plugin, sizeof(active_plugin))) {
		/* run tests for currently active plugin */
		plugin_name = active_plugin;
	}

	if (plugin_name) {
		for (i = 0; i < NUM_PLUGINS; i++)
			if (streq(testsuite[i].plugin, plugin_name)) {
				ok = run_tests(testsuite[i].testcases,
					       testsuite[i].num_cases,
					       testsuite[i].plugin);
				printf("** Result: %d ok, %d failed.\n",
				       ok, testsuite[i].num_cases - ok);
				return ok == testsuite[i].num_cases ? 0 : 3;
			}
		fprintf(stderr, "** Unknown plugin: '%s'\n", plugin_name);
		usage("");
		return 1;
	} else {
		usage("** Active plugin unknown");
		return 2;
	}
}
