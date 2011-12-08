#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>

#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "max.h"

int main()
{
	maxhandle_t hand;
	exception_t * e = NULL;
	
	max_init(&hand);
	max_connectlocal(&hand, &e);
	
	if (e != NULL)
	{
		printf("ERROR: (%d) %s\n", e->code, e->message);
		return 1;
	}
	printf("SUCCESS: conected\n");
	
	
	
	return_t r;
	
	bool s = max_syscall(&hand, &e, &r, "kernel_installed", "i:v", 2, 3);
	if (e != NULL)
	{
		printf("ERROR: (%d) %s\n", e->code, e->message);
		return 1;
	}
	printf("SYSCALL %d, %c, %d\n", s, r.type, r.data.t_integer);
	
	return 0;
}

#if 0
int main()
{
	
	maxhandle_t * hand = malloc(sizeof(maxhandle_t));
	max_init(hand);
	max_connectlocal(hand);
	max_settimeout(hand, 1);
	
	if (max_iserror(hand))
	{
		int code;
		const char * msg;
		
		code = max_error(hand, &msg);
		printf("ERROR: (%d) %s\n", code, msg);
		return 1;
	}
	else
	{
		printf("SUCCESS: conected\n");
	}
	
	
	
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1)
	{
		printf("Could not create datagram socket: %s\n", strerror(errno));
		return -1;
	}

	struct sockaddr_in addr;
	ZERO(addr);

	addr.sin_family = AF_INET;
	addr.sin_port = htons(10301);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		printf("Could not bind datagram to port %d: %s\n", 10301, strerror(errno));
		close(sock);
		return -1;
	}
	
	while (1)
	{
		int buf[4] = {0,0,0,0};
		
		int r = recvfrom(sock, buf, sizeof(buf), 0, NULL, NULL);
		if (r != sizeof(buf))
		{
			printf("Could not receive all data from host!\n");
			continue;
		}
		
		if (buf[0] != 0xA5A5A5A5)
		{
			printf("Framing error!\n");
			continue;
		}
		
		//printf("Got data! %d %d %d\n", buf[1], buf[2], buf[3]);
		printf(".");
		
		max_syscall(hand, "doio", "v:ii", 0, buf[1]);	// Throttle
		max_syscall(hand, "doio", "v:ii", 1, buf[2]);	// Stearing
		max_syscall(hand, "doio", "v:ii", 2, buf[3]);	// Aux
		
	}
	
	max_close(hand);

	return 0;
}
#endif

