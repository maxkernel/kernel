#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>


#include "unittest.h"

const int64_t kernel_timestamp()
{
	return 0LL;
}


static int term_width = 0;
static char term_teststr[LINEBUF];
static FILE * log = NULL;


void printheader(const char * title)
{
	int i=0;
	printf("%s ", title);
	for (; i<term_width-(strlen(title)+1); i++)
		printf("=");
	printf("\n");
}

void printtest(const char * str, bool pass)
{
#define PASS	" [ \033[92mPASS\033[0m ] "
#define FAIL	" [ \033[91mFAIL\033[0m ] "

	printf(term_teststr, str, pass ? PASS : FAIL);

}


void logheader(const char * title)
{
	if (log != NULL)
	{
		fprintf(log, "==> %s\n", title);
	}
}

void logtest(const char * str, bool pass)
{
	if (log != NULL)
	{
		fprintf(log, " [ %s ] %s\n", pass? "PASS" : "FAIL", str);
	}
}


void module(const char * modname)
{
	printheader(modname);
	logheader(modname);
	
}

void __assert(bool val, const char * valstr, const char * desc)
{
	char line[LINEBUF];
	if (val)
	{
		sprintf(line, "%s", desc);
	}
	else
	{
		sprintf(line, "%s (%s)", desc, valstr);
	}
	
	printtest(line, val);
	logtest(line, val);
}

int main()
{
	{ // Get the terminal width
		struct winsize w;
		ioctl(0, TIOCGWINSZ, &w);
		term_width = w.ws_col;
		sprintf(term_teststr, " %%-%ds %%s\n", term_width-12);
	}
	
	{ // Open the log file
		log = fopen(LOGFILE, "w");
		if (log == NULL)
		{
			fprintf(stderr, "ERROR: Could not open log file: %s\n", LOGFILE);
		}
	}
	
	// Run through tests
	test_serialize();
	
	
	return 0;
}


