#include "commtypes.h"

#include <assert.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netinet/tcp.h>

#include "bench_config.h"
#include "taslimited.h"
#include "replicas.h"

#define DELAY 1

int serverfd, got_reply = 1;

struct replica replica;
struct typed_pipe trans_pipes[2];
int initCS(){
	struct replica* r_p = (struct replica *) &replica;
	initReplicas(r_p, 1, "plumber", 10);

	struct vote_pipe new_pipes[2];
 	convertTypedToVote(trans_pipes, 2, new_pipes);
  	createPipes(r_p, 1, new_pipes, 2);

  	struct typed_pipe pipes[2];
  	convertVoteToTyped(r_p->rep_pipes, 2, pipes);

  	char *argv[2];
  	argv[0] = serializePipe(pipes[0]);
  	argv[1] = serializePipe(pipes[1]);
  	debug_print("Args from bench: %s: %s %d %d\n", argv[0], MESSAGE_T[pipes[0].type], pipes[0].fd_in, pipes[0].fd_out);
  	debug_print("Args from bench: %s: %s %d %d\n", argv[1], MESSAGE_T[pipes[1].type], pipes[1].fd_in, pipes[1].fd_out);

  	return 0;
}

void sig_handler(int signum)
{
        if ( signum == SIGURG )
        {   char c;
                recv(serverfd, &c, sizeof(c), MSG_OOB);
                got_reply = ( c == 'Y' );                       //Reply received.
                fprintf(stderr,"Heartbeat Detected\n");
        }
        else if ( signum == SIGALRM )
                if ( got_reply )
                {
                        send(serverfd, "?", 1, MSG_OOB);        //Send to server a request to check for uptime.
                        alarm(DELAY);                           //Wait the amount of time of "DELAY".
                        got_reply = 0;
                }
                else{
                        fprintf(stderr, "Error: Heartbeat Lost\n");
                        system("../stage_control/basic 192.168.69.140");
                        fprintf(stderr, "Error: How did i get here?\n");
                }
}
	
	
int clientComm(int count, char *strings[]){
	struct sockaddr_in addr;
	struct sigaction act;
	int bytes;
	char line[100];

	if(count != 3){
		printf("Parameters: %s <address> <port>\n", strings[0]);
		exit(0);
	}
	bzero(&act, sizeof(act));
	act.sa_handler = sig_handler;
	act.sa_flgas = SA_RESTART;
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

int main(int count, char *strings[]){
	pid_t pid = 0;

	if(initCS() != 0){ 
		printf("ERROR: Initiation of CS failed"); 
	}
	
	pid = fork();
		
	//Fork is successful
	if(pid >= 0){
		//Current process is a child
		if(pid = 0){
			//Required to create and traverse nodes for parameters from the cfg file?
			if(-1 == exec("plumber", )){
				printf("CS Initial Fork");
			}
		}
		//Current process is a parent
		else{
			clientComm(count, strings);
		}
	} else{
		printf("CS - Error while forking");
		return -1;
	}

	/*
	char command[500];
	if(initCS() < 0){
		printf("ERROR: Failure in initCS setup.");
		return -1;
	}
	
	strcpy(command, "../stage_control/basic ");
	strcat(command, argv[1]);
	strcat(command, " 1234");
	system(command);
	*/
}
