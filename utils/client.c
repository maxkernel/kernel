#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <argp.h>

#define FIFO		"/tmp/maxclient.fifo"
#define CMDFILE		"/tmp/maxclient.cmd"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

static volatile pid_t CHILD = 0;

static struct {
	int autostart;
	int command;
	int daemon;
	int kill;
} args = { 0 };

static struct argp_option arg_opts[] = {
	{ 0, 'a', 0, 0, "execute autostart program", 0 },
	{ 0, 'c', 0, 0, "if another max-client program is executing, print the last command used", 0 },
	{ 0, 'd', 0, 0, "start program as daemon", 0 },
	{ 0, 'k', 0, 0, "kill the currently running program if exists", 0 },
	{ 0 }
};

static error_t parse_args(int key, char * arg, struct argp_state * state);
static struct argp argp = { arg_opts, parse_args, 0, 0 };

static error_t parse_args(int key, char * arg, struct argp_state * state)
{
	switch (key)
	{
		case 'a':	args.autostart = 1;		break;
		case 'c':	args.command = 1;		break;
		case 'd':	args.daemon = 1;		break;
		case 'k':	args.kill = 1;			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static void daemonize()
{
	pid_t pid, sid;
	
    	/* Fork off the parent process */
	pid = fork();
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}
	/* If we got a good PID, then we can exit the parent process. */
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	/* At this point we are executing as the child process */

	/* Create a new SID for the child process */
	sid = setsid();
	if (sid < 0) {
		exit(EXIT_FAILURE);
	}

	/* Redirect standard files to /dev/null */
	freopen( "/dev/null", "r", stdin);
	freopen( "/dev/null", "w", stdout);
	freopen( "/dev/null", "w", stderr);
}

static void dosyscalls()
{
	//execute some syscalls to keep rover safe and undamaged
	system("max-syscall motoroff");
	system("max-syscall alloff");
}

static void postexec()
{
	dosyscalls();
	unlink(FIFO);
	unlink(CMDFILE);
}

static void startchild(char ** args)
{
	//write command to CMDFILE
	int cfp = open(CMDFILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	int i=0;
	while (args[i] != NULL)
	{
		write(cfp, args[i], strlen(args[i]));
		write(cfp, " ", 1);
		i++;
	}
	close(cfp);
	chmod(CMDFILE, 0644);


	CHILD = fork();
	if (CHILD == 0)
	{
		//child
		
		//sleep for a bit so parent can catch up
		usleep(500);
		
		execvp(args[0], args);
		fprintf(stderr, "* Fall through! Could not execute %s\n", args[0]);
		exit(EXIT_FAILURE);
	}
	else if (CHILD < 0)
	{
		fprintf(stderr, "* Could not fork\n");
		exit(EXIT_FAILURE);
	}
}

static void killall()
{
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	
	do
	{
		killpg(getpgrp(), SIGINT);
		usleep(100000);
		killpg(getpgrp(), SIGTERM);
		usleep(100000);
	}
	while (CHILD != 0);
	
	exit(EXIT_SUCCESS);
}

static void sigchld(int sig)
{
	pid_t chld = waitpid(0, NULL, WNOHANG);
	if (chld == CHILD)
	{
		CHILD = 0;
		killall();
	}
}

static void sigint(int sig)
{
	killall();
}

void waitonfifo()
{
	int i;
	int max_usleep = 1500000;
	int step = 10000;

	for (i=0; i<max_usleep/step; i++)
	{
		if (access(FIFO, F_OK) == -1)
		{
			//file doesn't exist
			return;
		}

		usleep(step);
	}
}


int main(int argc, char ** argv)
{
	int argi;
	argp_parse(&argp, argc, argv, ARGP_IN_ORDER, &argi, 0);
	
	int fifo_fp = open(FIFO, O_WRONLY | O_NONBLOCK);
	if (fifo_fp > 0)
	{
		if (args.command)
		{
			int cfp = open(CMDFILE, O_RDONLY);
			if (cfp < 0)
			{
				fprintf(stderr, "* max-client seems to be executing, but could not read command\n");
			}
			else
			{
				char buf[250];
				int r = read(cfp, buf, 249);
				buf[r] = '\0';
				close(cfp);

				fprintf(stdout, "%s\n", buf);
			}
		}

		if (args.kill)
		{
			//tell the process to kill itself by writing a 'k' to the fifo
			write(fifo_fp, "k", 1);
			waitonfifo();
		}
		else if (!args.command)
		{
			fprintf(stderr, "* max-client is currently executing\n");
			exit(EXIT_FAILURE);
		}

		if (args.command)
		{
			exit(EXIT_SUCCESS);
		}
	}
	close(fifo_fp);

	if (args.command)
	{
		fprintf(stderr, "* max-client is not currently executing\n");
		exit(EXIT_FAILURE);
	}

	if (argi == argc && !args.autostart)
	{
		if (!args.kill)
			fprintf(stdout, "* nothing to do\n");
		exit(EXIT_SUCCESS);
	}

	if (args.autostart)
	{
		//check existence
		if (access(INSTALL "/autostart", F_OK) == -1)
		{
			fprintf(stderr, "* autostart file does not exist\n");
			exit(EXIT_FAILURE);
		}
	}

	if (args.daemon)
	{
		daemonize();
	}
	else if (setpgrp() < 0)
	{
		fprintf(stderr, "* Could not set process group id\n");
		exit(EXIT_FAILURE);
	}
	
	//
	atexit(postexec);

	//reg signals
	signal(SIGINT, sigint);
	signal(SIGCHLD, sigchld);
	signal(SIGHUP, SIG_IGN);

	//execute client
	if (args.autostart)
	{
		int afp = open(INSTALL "/autostart", O_RDONLY);
		if (afp < 0)
		{
			fprintf(stderr, "* autostart file exists, but could not access it\n");
			exit(EXIT_FAILURE);
		}

		//read in autostart
		char buf[250];
		int r = read(afp, buf, 249);
		buf[r] = '\0';
		close(afp);

		char ** args = malloc(sizeof(char *) * 100);
		int i=1;
		args[0] = strtok(buf, " \t\n");
		if (args[0] == NULL)
		{
			fprintf(stdout, "* nothing to do\n");
			exit(EXIT_SUCCESS);
		}

		do {
			args[i] = strtok(NULL, " \t\n");
		} while (args[i++] != NULL);

		//call start child on the parsed args
		startchild(args);
		free(args);
	}
	else
	{
		startchild(argv + argi);
	}

	//make the fifo
	mknod(FIFO, S_IFIFO | 0622, 0);
	chmod(FIFO, 0622);

	while (1)
	{
		fifo_fp = open(FIFO, O_RDONLY);
		if (fifo_fp < 0)
		{
			fprintf(stderr, "Could not open FIFO %s: %s\n", FIFO, strerror(errno));
			exit(EXIT_FAILURE);
		}

		int ret;
		char data;
		while ((ret = read(fifo_fp, &data, 1)) > 0)
		{
			switch (data)
			{
				case 'k':
					killall();
					return 0;

				default:
					fprintf(stderr, "Unknown command received in fifo: '%c'\n", data);
					break;
			}
		}

		if (ret < 0)
		{
			fprintf(stderr, "Error in FIFO read loop: %s\n", strerror(errno));
			break;
		}

		close(fifo_fp);
	}

	return 0;
}

