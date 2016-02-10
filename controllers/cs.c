#include "commtypes.h"

#include <assert.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "bench_config.h"
#include "taslimited.h"
#include "replicas.h"
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

  	forkReplicas(r_p, 1, 2, argv);

  	return 0;
}

int main(int argc, const char **argv){
	pid_t pid = 0;

	pid = fork();
		
	//Fork is successful
	if(pid >= 0){
		//Current process is a child
		if(pid = 0){
			//?
			if(-1 == exec("plumber", "plumber_parameters")){
				printf("CS Initial Fork");
			}
		}
		//Current process is a parent
		else{
			//Connection to the server made here?
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
