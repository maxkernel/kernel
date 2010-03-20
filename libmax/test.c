#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>

#include "max.h"

int main()
{	
	maxhandle_t * hand = max_remote("localhost");
	
	if (max_iserror(hand))
	{
		printf("ERROR: %s\n", max_error(hand));
		return 1;
	}
	
	char * s = NULL;
	double d = 0.0;
	
	s = NULL;
	printf("Calling kernel_id()\n");
	printf("Result: %d", max_syscall(hand, "kernel_id", "s:v", &s));
	printf(" (%s)\n", max_error(hand));
	printf("Returned: %s\n", s);
	free(s);
	
	printf("=======\n");
	
	d = 0.0;
	printf("Calling math_eval(\"cos( 2 * pi )\")\n");
	printf("Result: %d", max_syscall(hand, "math_eval", "d:s", &d, "cos( 2 * pi )"));
	printf(" (%s)\n", max_error(hand));
	printf("Returned %f\n", d);
	
	return 0;
}
