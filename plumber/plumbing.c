/*
 * James Marshall 
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "plumbing.h"

// True if succeeds, false otherwise.
bool add_node(struct nodelist* nodes, char* Name, char* Value, replication_t rep_type, char* voter_name) {
	// if current is null, add new node
	if (nodes->current == NULL) {
		struct node *new_node = malloc(sizeof(struct node));
		new_node->name = malloc(strlen(Name));
		new_node->value = malloc(strlen(Value));
		strcpy(new_node->name, Name);
		strcpy(new_node->value, Value);

		// Pipes
		new_node->pipe_count = 0;

		// Set replication strategy and voter name (if one)
		new_node->rep_strat = rep_type;
		if (rep_type != NONE) {
			new_node->voter_name = malloc(strlen(voter_name));
			strcpy(new_node->voter_name, voter_name);
		} else {
			new_node->voter_name = NULL;
		}

		nodes->current = new_node;
		nodes->next = malloc(sizeof(struct nodelist));		
		return true;
	} else if (strcmp(Name, nodes->current->name) == 0) {
		// Node already exists, bail
		printf("PLUMBING ERROR: Re-defining component: %s\n", Name);
		return false;
	} else {
		return add_node(nodes->next, Name, Value, rep_type, voter_name);
	}
}

struct node* get_node(struct nodelist* nodes, char* Name) {
	if (nodes->current == NULL) {
		printf("PLUMBING ERROR: get_node, request nodes doesn't exist: %s\n", Name);
		return NULL;
	} else if (strcmp(Name, nodes->current->name) == 0) {
		return nodes->current;
	} else {
		return get_node(nodes->next, Name);
	}
}

// stupid bench making everything a pain
// Bench already has fds, one passed in should be 0
void link_bench(struct node* n, comm_message_t type, int fd_in, int fd_out) {
	n->pipes[n->pipe_count].type = type;
	n->pipes[n->pipe_count].fd_in = fd_in;
	n->pipes[n->pipe_count].fd_out = fd_out;
	n->pipe_count++;
}

void link_node(comm_message_t type, struct node* fromNode, struct node* toNode) {
	// create pipe
	int pipe_fds[2];
	if (pipe(pipe_fds) == -1) {
		printf("Plumber pipe error\n");
	} else {
		// give half to fromNode
		fromNode->pipes[fromNode->pipe_count].type = type;
		fromNode->pipes[fromNode->pipe_count].fd_in = 0;
		fromNode->pipes[fromNode->pipe_count].fd_out = pipe_fds[1];
		fromNode->pipe_count++;
		// other half to toNode
		toNode->pipes[toNode->pipe_count].type = type;
		toNode->pipes[toNode->pipe_count].fd_in = pipe_fds[0];
		toNode->pipes[toNode->pipe_count].fd_out = 0;
		toNode->pipe_count++;
	}
}

void print_nodes(struct nodelist* nodes)
{
	if (nodes->current == NULL) {
		printf("XXX\n");
		return;
	} else {
		printf("%s\n", nodes->current->name);
		print_nodes(nodes->next);
	}
}

// TODO: compare to "forkSingleReplica" in replicas.cpp
int launch_node(struct nodelist* nodes) {
	pid_t currentPID = 0;
	char** rep_argv;
	// TODO: handle args
	int i;

	struct node* curr = nodes->current;

	if (curr != NULL) {
		int rep_count = 0;
		int other_arg = 0;
		if (curr->rep_strat == NONE) {
			// launch with no replication
			rep_count = 2 + curr->pipe_count;
			rep_argv = malloc(sizeof(char *) * rep_count);
			rep_argv[0] = curr->value;
			other_arg = 1;
			rep_argv[rep_count - 1] = NULL;
		} else if (curr->rep_strat == DMR) {
			printf("pb.y: No support for DMR\n");
		} else if (curr->rep_strat == TMR) {
			// launch with voter
			int rep_count = 3 + curr->pipe_count;
			rep_argv = malloc(sizeof(char *) * rep_count);
			rep_argv[0] = curr->voter_name;
			rep_argv[1] = curr->value;
			other_arg = 2;
			rep_argv[rep_count - 1] = NULL;
		}


		for (i = other_arg; i < curr->pipe_count + other_arg; i++) {
			rep_argv[i] = serializePipe(curr->pipes[i - other_arg]);
		}

		currentPID = fork();

		if (currentPID >= 0) { // Successful fork
			if (currentPID == 0) { // Child process
				if (-1 == execv(rep_argv[0], rep_argv)) {
					printf("File: %s\n", rep_argv[0]);
					perror("Plumber: EXEC ERROR!");
					free(rep_argv);
					return -1;
				}
			} else { // Parent Process
				launch_node(nodes->next);
			}
		} else {
			printf("Fork error!\n");
			free(rep_argv);
			return -1;
		}
		// TODO: Need to free all pointers inside too, no?
		free(rep_argv);
	} // curr == NULL, okay.
}