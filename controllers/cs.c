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

// restart timer fd
char timeout_byte[1] = {'*'};
char heartbeat_byte[1] = {'h'};
int timeout_fd[2];
int heartbeat_fd[2];
timer_t timerid;
struct itimerspec its;
int client;
struct sigaction act;

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

void timeout_sighandler(int signum){
	write(timeout_fd[1], timeout_byte, 1);
}

int initTimer(){
	struct sigevent sev;
	struct sigaction sa;
	sigset_t_ mask;

	// Setup the timeout pipe
	if (pipe(timeout_fd) == -1) {
		perror("Timeout pipe create fail");
		return -1;
	}

	if (pipe(heartbeat_fd) == -1){
		perror("Heartbeat pipe create fail");
		return -1;
	}

	// Setup the signal handler
	if (signal(SIG, timeout_sighandler) == SIG_ERR) {
		perror("sigaction failed");
		return -1;
	}

	// Make sure that the timeout signal isn't blocked (will be by default).
	sigemptyset(&mask);
	sigaddset(&mask, SIG);
	if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) {
		perror("sigprockmask failed");
		return -1;
	}

	// Create the timer
	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIG;
	sev.sigev_value.sival_ptr = &timerid;
	if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) == -1) {
		perror("timer_create failed");
		return -1;
	}

	return 0;

}


//WARNING - COMM SERVER AND RIS ARE REVERSED, RIS SHOULD DETECT WHEN CS IS DOWN, NOT THE REVERSE.
void sig_handler(int signum){
	if(signum == SIGURG){
		char c;
		recv(client, &c, sizeof(c), MSG_OOB);
		if(c == '?'){
			send(client, "Y", 1, MSG_OOB);
		}
	}else if(signum == SIGCHLD){
		wait(0);
	}	
}

void servlet(void){
	int bytes;
	char buffer[1024];
	
	bzero(&act, sizeof(act));
	act.sa_handler = sig_handler;
        act.sa_flags = SA_RESTART;
        sigaction(SIGURG, &act, 0);     /* connect SIGURG signal */
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

int clientComm(int count, char *strings[]){
	int sd, client_len;
	struct sockaddr_in addr;
	struct sockaddr_in client_address;
	char clntName[INET_ADDRSTRLEN];

	if ( count != 2 )
	{
		printf("usage: %s <port>\n", strings[0]);     //Command line input error checking.
		exit(0);
		//strings[1] = '1234';
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
		client = accept(sd, (struct sockaddr *) &client_address, &client_len);
		printf("Client accepted.\n");

		if(inet_ntop(AF_INET, &client_address.sin_addr.s_addr, clntName, sizeof(clntName))!=NULL){
			printf("%s%d\n", clntName, ntohs(client_address.sin_port));
		}
		else{
			printf("Address Not Found\n");
		}

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

int main(int count, char *strings[]){
	pid_t pid = 0;

	if(initTimer() < 0){
		puts("ERROR: initTimer failed.\n");
		return -1;
	}

	//Arm Timer
	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = 0;
	its.it_value.tv_sec = 2;
	its.it_value.tv_nsec = 0;

	if(timer_settime(timerid, 0, &its, NULL) == -1 ){
		perror("timer_settime failed");
	}

	if(initCS() != 0){ 
		printf("ERROR: Initiation of CS failed"); 
	}
	//---Switch to forkReplicas 
	pid = fork();

	//Fork is successful
	if(pid >= 0){
		//Current process is a child
		if(pid = 0){
			//Required to create and traverse nodes for parameters from the cfg file?
			if(-1 == execv("plumber", 2, 2, pipes[0].fd_in, pipes[1].fd_out )){
				printf("CS Initial Fork");
			}
		}
		//Current process is a parent
		else{
			clientComm(count, strings);

			while(1){
				int retval = 0;

				struct timeval select_timeout;
				fd_set select_set;	
				//Timeout for select call.
				select_timeout.tv_sec = 1;
				select_timeout.tv_usec = 0;

				FD_ZERO(&select_set);
				//Check for timeouts
				FD_SET(timeout_fd[0], &select_set);

				//Check for hearbeats
				FD_SET(heartbeat_fd[0], &select_set);

				/*------------RIS check.----------
				  FD_SET(sim_server_fd, &select_set);
				  ---------------------------------*/

				/*-----Control program check------
				  FD_SET(control_fd, &select_set);
				  --------------------------------*/

				// This will wait at least timeout until return. Returns earlier if something has data.
				retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);

				if (retval > 0) {
					// One of the fds has data to read
					// Figure out with one with FD_ISSET

					// Check for failed replica (time out)
					if (FD_ISSET(timeout_fd[0], &select_set)) {
						char theByte = 'a';
						// Don't forget to read the character to unset select
						read(timeout_fd[0], &theByte, sizeof(char));

						printf("Timeout expired: %c\n", theByte); // What is the answer? Print as %d.

						// rearm the timer
						its.it_interval.tv_sec = 0;
						its.it_interval.tv_nsec = 0;
						its.it_value.tv_sec = 2;
						its.it_value.tv_nsec = 0;

						if (timer_settime(timerid, 0, &its, NULL) == -1) {
							perror("timer_settime failed");
						}
					}

					// Check for data from the Robot Interface Server
					// if (FD_ISSET(sim_server_fd, &select_set)) {
					// These are simulator data that should be written to the control program.
					// You may want to reset the timer here (since the Robot Interface Server is alive).
					// }

					// Check for data from the control program
					// if (FD_ISSET(control_fd, &select_set)) {
					// These are commands from the control program that should be written to the Robot Interface Server.
					// }

					// Check for if heartbeat message was lost
					// if(FD_ISSET(heartbeat_fd[0], &select_set)){
					// If heartbeat is lost, take over here.
					// }
				}

			}
		}
	} else{
		printf("CS - Error while forking");
		return -1;
	}
}
