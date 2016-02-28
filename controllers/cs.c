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

bool checkSync(void) {
	int r_index, p_index;
	bool nsync = true;

	// check each that for each pipe, each replica has the same number of votes
	for (p_index = 0; p_index < pipe_count; p_index++) {
		int votes = replicas[0].vot_pipes[p_index].buff_count;
		for (r_index = 1; r_index < rep_count; r_index++) {
			if (votes != replicas[r_index].vot_pipes[p_index].buff_count) {
				nsync = false;
			}
		}
	}
	return nsync;
}

int main(int count, char *strings[]){
	pid_t pid = 0;
	int retval = 0;
	fd_set select_set;
	int p_index, r_index;
	select_timeout.tv_sec = 0;
	select_timeout.tv_usec = 50000;
	FD_ZERO(&select_set);
	bool check_inputs;

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
			/*--------------------------Check external input pipes----------------------
			check_inputs = checkSync();

			if(check_inputs){
				for(p_index = 0; p_index < pipe_count; p_index++){
					if(ext_pipes[p_index].fd_in != 0){
						int e_pip_fd = ext_pipes[p_index].fd_in;
						FD_SET(e_pipe_fd, &select_set);
					}
				}
			}
			---------------------------------------------------------------------------*/
			
			/*------------------------Check pipes from replicas--------------------------
			for(p_index = 0; p_index < pipe_count; p_index++){
				for(r_index = 0; r_index < rep_count; r_index++){
					int rep_pipe_fd = replicas[r_index].vot_pipes[p_index].fd_in;
					if(rep_pipe_fd != 0){
						FD_SET(rep_pipe_fd, &select_set);
					}
				}
			}
			---------------------------------------------------------------------------*/

			//Wait until timeout period, unless something has data to be read
			retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);
			//This should be repeated in a separate function call, move later
			if(retval > 0){
				//Check for data from external sources
				for(p_index = 0; p_index < pipe_count; p_index++){
					int read_fd = ext_pipes[p_index].fd_in;
					if(read_fd !=0){
						if (FD_ISSET(read_fd, &select_set)) {
							ext_pipes[p_index].buff_count = read(read_fd, ext_pipes[p_index].buffer, MAX_VOTE_PIPE_BUFF);
							if (ext_pipes[p_index].buff_count > 0) { // Read may still have been interrupted
								processData(&(ext_pipes[p_index]), p_index);
							} else if (ext_pipes[p_index].buff_count < 0) {
								printf("Voter - Controller %s pipe %d\n", controller_name, p_index);
								perror("Voter - read error on external pipe");
							} else {
								printf("Voter - Controller %s pipe %d\n", controller_name, p_index);
								perror("Voter - read == 0 on external pipe");
							}
						}

					}
				}

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
}
