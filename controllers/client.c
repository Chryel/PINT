#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netinet/tcp.h>

#define DELAY	1 //seconds

int serverfd, got_reply = 1;

void sig_handler(int signum)
{
	if ( signum == SIGURG )
	{   char c;
		recv(serverfd, &c, sizeof(c), MSG_OOB);
		got_reply = ( c == 'Y' ); 			//Reply received.
		fprintf(stderr,"Heartbeat Detected\n");
	}
	else if ( signum == SIGALRM )
		if ( got_reply )
		{
			send(serverfd, "?", 1, MSG_OOB);	//Send to server a request to check for uptime.
			alarm(DELAY);				//Wait the amount of time of "DELAY".
			got_reply = 0;
		}
		else
			fprintf(stderr, "Error: Heartbeat Lost");
}

int main(int count, char *strings[])
{	struct sockaddr_in addr;
	struct sigaction act;
	int bytes;
	char line[100];

	if ( count != 3 )
	{
		printf("usage: %s <addr> <port>\n", strings[0]);	//Command line input format is incorrect.
		exit(0);
	}
	bzero(&act, sizeof(act));
	act.sa_handler = sig_handler;
	act.sa_flags = SA_RESTART;
	sigaction(SIGURG, &act, 0);
	sigaction(SIGALRM, &act, 0);

	serverfd = socket(PF_INET, SOCK_STREAM, 0);
	//Claim signals for SIGIO and SIGURG.
	if ( fcntl(serverfd, F_SETOWN, getpid()) != 0 )
		perror("Can't claim SIGURG and SIGIO");
	//Standard setup for internet connection.
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(strings[2]));
	inet_aton(strings[1], &addr.sin_addr);
	if ( connect(serverfd, (struct sockaddr*)&addr, sizeof(addr)) == 0 )
	{
		alarm(DELAY);
		do
		{
			gets(line);
			printf("send [%s]\n", line);
			send(serverfd, line, strlen(line), 0);
			bytes = recv(serverfd, line, sizeof(line), 0);
		}
		while ( bytes > 0 );
	}
	else
		perror("connect failed");
	close(serverfd);
	return 0;
}

