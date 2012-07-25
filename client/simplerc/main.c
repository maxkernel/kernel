#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <SDL.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//#include <max.h>

int main(int argc, char ** argv)
{
	if (argc != 2)
	{
		printf("Error: ip address not specified\n");
		return 1;
	}
	
	SDL_Init(SDL_INIT_JOYSTICK);
	SDL_JoystickEventState(SDL_DISABLE);
	
	int numsticks = SDL_NumJoysticks();
	int usestick = 0;
	
	printf("Found %d Joystick(s):\n", numsticks);
	int i=0;
	for (; i<numsticks; i++)
	{
		printf("(%d) %s\n", i, SDL_JoystickName(i));
	}
	
	printf("....\n");
	printf("Using joystick %d\n", usestick);
	
	SDL_Joystick * joy = SDL_JoystickOpen(usestick);
	if (joy == NULL)
	{
		printf("Error: Could not open joystick %d\n", usestick);
		fflush(stdout);
		return 0;
	}
	
	printf("Joystick opened\n");
	printf("Number of Axes: %d\n", SDL_JoystickNumAxes(joy));
	printf("Number of Buttons: %d\n", SDL_JoystickNumButtons(joy));
	printf("Number of Balls: %d\n", SDL_JoystickNumBalls(joy));
	
	printf("....\n");
	fflush(stdout);
	
	int sockfd;
	struct sockaddr_in servaddr,cliaddr;

	sockfd=socket(AF_INET,SOCK_DGRAM,0);
	
	printf("Connecting to %s\n", argv[1]);
	fflush(stdout);

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr=inet_addr(argv[1]);
	servaddr.sin_port=htons(10301);

	char buf[500];
	int lastdir = 1;
	
	while (1)
	{	
		SDL_JoystickUpdate();
		double x = (double)SDL_JoystickGetAxis(joy, 0) / (double)0x7FFF;
		double y = (double)SDL_JoystickGetAxis(joy, 1) / (double)0x7FFF;
		double z = -(double)SDL_JoystickGetAxis(joy, 3) / (double)0x7FFF;
		double r = (double)SDL_JoystickGetAxis(joy, 2) / (double)0x7FFF;
		//printf("X: %f\tY: %f\tZ: %f\tR: %f\n", x, y, z, r);
		
		double speedlimit = 1;
		
		//sprintf(buf, "2%f", 5.0*M_PI/36.0*r);
		sprintf(buf, "2%f", r);
		sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
		
		sprintf(buf, "0%f", -y*speedlimit);
		sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
		
		usleep(16000);
	}
	
	
	//clean up
	SDL_JoystickClose(joy);
	
	return 0;
}

