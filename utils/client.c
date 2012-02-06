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
#include <ftw.h>

#include <aul/common.h>
#include <aul/string.h>

#define FIFO		"/tmp/maxclient.fifo"
#define CMDFILE		"/tmp/maxclient.cmd"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

static volatile pid_t child = 0;
static char * tempdir = NULL;

static struct
{
	int printcommand;
	int forkdaemon;
	int killother;
	char * archive;
	char * directory;
} args = { 0 };

static struct argp_option arg_opts[] = {
	{ "command",    'c',    0,          0, "if another max-client program is executing, print the last command used", 0 },
	{ "daemon",     'd',    0,          0, "start program as daemon", 0 },
	{ "kill",       'k',    0,          0, "kill the currently running program if exists", 0 },
	{ "archive",    'x',    "file",     0, "extract given file into a temporary directory and execute within that directory", 0 },
	{ "directory",  'C',    "dir",      0, "change to the given directory prior to executing command", 0 },
	{ 0 }
};

bool extract(char * path, char * dir);

static error_t parse_args(int key, char * arg, struct argp_state * state);
static struct argp argp = { arg_opts, parse_args, 0, 0 };

static error_t parse_args(int key, char * arg, struct argp_state * state)
{
	switch (key)
	{
		case 'c':	args.printcommand = 1;		    break;
		case 'd':	args.forkdaemon = 1;		    break;
		case 'k':	args.killother = 1;			    break;
		case 'x':   args.archive = strdup(arg);     break;
		case 'C':   args.directory = strdup(arg);   break;

		default:
			return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static void daemonize()
{
	pid_t pid, sid;
	
    // Fork off the parent process
	pid = fork();
	if (pid < 0)
	{
		exit(EXIT_FAILURE);
	}
	
	// If we got a good PID, then we can exit the parent process.
	if (pid > 0)
	{
		exit(EXIT_SUCCESS);
	}

	// At this point we are executing as the child process

	// Create a new SID for the child process
	sid = setsid();
	if (sid < 0)
	{
		exit(EXIT_FAILURE);
	}

	// Redirect standard files to /dev/null
	freopen( "/dev/null", "r", stdin);
	freopen( "/dev/null", "w", stdout);
	freopen( "/dev/null", "w", stderr);
}

static void postexec()
{
	unlink(FIFO);
	unlink(CMDFILE);
}

static void startchild(char ** args)
{
	// Write command to CMDFILE
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


	child = fork();
	if (child == 0)
	{
		// Child
		
		// Sleep for a bit so parent can catch up
		usleep(500);
		
		execvp(args[0], args);
		fprintf(stderr, "* Fall through! Could not execute %s\n", args[0]);
		exit(EXIT_FAILURE);
	}
	else if (child < 0)
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
	while (child != 0);
	
	if (tempdir != NULL)
	{
		int rmfiles(const char * path, const struct stat * ptr, int flag)
		{
			if (flag != FTW_D) remove(path);
			return 0;
		}
		
		int rmdirs(const char * path, const struct stat * ptr, int flag)
		{
			if (flag == FTW_D) remove(path);
			return 0;
		}
		
		// Remove all the files in tempdir
		ftw(tempdir, rmfiles, 25);
		
		// Remove all the dirs in tempdir
		ftw(tempdir, rmdirs, 25);
		
		// Now remove tempdir
		remove(tempdir);
	}
	
	exit(EXIT_SUCCESS);
}

static void sigchld(int sig)
{
	pid_t chld = waitpid(0, NULL, WNOHANG);
	if (chld == child)
	{
		child = 0;
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
	
	if (args.directory != NULL)
	{
		if (chdir(args.directory) != 0)
		{
			fprintf(stderr, "* could not set working directory to %s\n", args.directory);
			exit(EXIT_FAILURE);
		}
	}
	
	if (args.archive != NULL)
	{
		char t[] = "/tmp/max-client.XXXXXX";
		tempdir = mkdtemp(t);
		
		if (!extract(args.archive, tempdir))
		{
		    fprintf(stderr, "* could not extract archive %s\n", args.archive);
			exit(EXIT_FAILURE);
		}
		
		if (chdir(tempdir) != 0)
		{
			fprintf(stderr, "* could not set working directory to %s\n", tempdir);
			exit(EXIT_FAILURE);
		}
	}
	
	int fifo_fp = open(FIFO, O_WRONLY | O_NONBLOCK);
	if (fifo_fp > 0)
	{
		if (args.printcommand)
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

		if (args.killother)
		{
			//tell the process to kill itself by writing a 'k' to the fifo
			write(fifo_fp, "k", 1);
			waitonfifo();
		}
		else if (!args.printcommand)
		{
			fprintf(stderr, "* max-client is currently executing\n");
			exit(EXIT_FAILURE);
		}

		if (args.printcommand)
		{
			exit(EXIT_SUCCESS);
		}
	}
	close(fifo_fp);

	if (args.printcommand)
	{
		fprintf(stderr, "* max-client is not currently executing\n");
		exit(EXIT_FAILURE);
	}

	if (argi == argc)
	{
		if (!args.killother)
			fprintf(stdout, "* nothing to do\n");

		exit(EXIT_SUCCESS);
	}

	if (args.forkdaemon)
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

	// Register signals
	signal(SIGINT, sigint);
	signal(SIGCHLD, sigchld);
	signal(SIGHUP, SIG_IGN);

	// Execute client
	startchild(argv + argi);

	// Make the fifo
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

