#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <resolv.h>
#include <signal.h>
#include <pthread.h>

int client;
struct sigaction act;

//Catch and send heartbeat
void sig_handler(int signum)
{
	if ( signum == SIGURG )
	{   char c;
		recv(client, &c, sizeof(c), MSG_OOB);
		if ( c == '?' )				   	//If client asks if server is still up:
			send(client, "Y", 1, MSG_OOB);		//Reply that it is.
	}
	else if ( signum == SIGCHLD )
		wait(0);
}

//Process requests.
void servlet(void)
{	int bytes;
	char buffer[1024];

	bzero(&act, sizeof(act));
	act.sa_handler = sig_handler;
	act.sa_flags = SA_RESTART;
	sigaction(SIGURG, &act, 0);	/* connect SIGURG signal */
	if ( fcntl(client, F_SETOWN, getpid()) != 0 )
		perror("Can't claim SIGIO and SIGURG");
	do
	{
		bytes = recv(client, buffer, sizeof(buffer), 0);
		if ( bytes > 0 )
			send(client, buffer, bytes, 0);
	}
	while ( bytes > 0 );
	close(client);
	exit(0);
}

//Set up client and begin the heartbeat.
int main(int count, char *strings[])
{	int sd, client_len;
	struct sockaddr_in addr;
	struct sockaddr_in client_address;
	char clntName[INET_ADDRSTRLEN];

	if ( count != 2 )
	{
		//printf("usage: %s <port>\n", strings[0]);	//Command line input error checking.
		//exit(0);
		strings[1] = '1234';
	}
	bzero(&act, sizeof(act));
	act.sa_handler = sig_handler;
	act.sa_flags = SA_NOCLDSTOP | SA_RESTART;
	if ( sigaction(SIGCHLD, &act, 0) != 0 ){ perror("sigaction()"); }
	//Standard server setup.
	sd = socket(PF_INET, SOCK_STREAM, 0);
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(strings[1]));
	addr.sin_addr.s_addr = INADDR_ANY;
	if ( bind(sd, (struct sockaddr*)&addr, sizeof(addr)) != 0 ){ perror("bind()"); }
	listen(sd, 15);
	for (;;)
	{
		client = accept(sd, 0, 0);
		printf("Client accepted.\n");	

		/*	Attempting to find the IP address of the client through the socket connection.
			---Segmentation Fault---
		if(inet_ntop(AF_INET, &client_address.sin_addr.s_addr, clntName, sizeof(clntName))!=NULL){
			printf("%s/%s\n", clntName, '/', ntohs(client_address.sin_port));
		}
		else{
			printf("Address Not Found\n");
		}
		*/
		
		if ( client > 0 )
		{
			if ( fork() == 0 )
			{
				close(sd);
				servlet();
			}
			else
				close(client);
		}
		else
			perror("accept()");

	}
	close(sd);
	return 0;
}
