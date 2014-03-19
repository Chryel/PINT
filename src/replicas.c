#include "replicas.h"

int initReplicas(struct replica_group* rg, struct replica* reps, int num) {
  int index = 0;
  int flags = 0;

  rg->replicas = reps;
  rg->num = num;
  rg->nfds = 0;
  FD_ZERO((&rg->read_fds));
  
  // Init three replicas
  for (index = 0; index < rg->num; index++) {
    //    printf("Initing index: %d\n", index);
    if (pipe(rg->replicas[index].pipefd) == -1) {
      printf("Pipe error!\n");
      return 0;
    }

    // Need to set to be non-blocking for reading.
    flags = fcntl(rg->replicas[index].pipefd[0], F_GETFL, 0);
    fcntl(rg->replicas[index].pipefd[0], F_SETFL, flags | O_NONBLOCK);

    // nfds should be the highes file descriptor, plus 1
    // TODO: This may have to be changed for when signal fd is added
    if (rg->replicas[index].pipefd[0] >= rg->nfds) {
      rg->nfds = rg->replicas[index].pipefd[0] + 1;
    }
    // Set to select on pipe's file descriptor
    FD_SET(rg->replicas[index].pipefd[0], &(rg->read_fds));

    rg->replicas[index].pid = -1;
    rg->replicas[index].priority = -1;
    //    rg->replicas[index].last_result = NULL;
    rg->replicas[index].status = RUNNING;
  }

  // TODO: errors?
  return 1;
}

void replicaCrash(struct replica_group* rg, pid_t pid) {
  int index;

  kill(pid, SIGKILL);
  for (index = 0; index < rg->num; index++) {
    if (rg->replicas[index].pid == pid) {
      rg->replicas[index].status = CRASHED;
    }
  }
}

// TODO: Belongs here?
int launchChildren(struct replica_group* rg) {
  pid_t currentPID = 0;
  int write_out = 0;
  int index = 0;

  // Fork children
  for (index = 0; index < rg->num; index++) {
    currentPID = fork();
    
    if (currentPID >= 0) { // Successful fork
      if (currentPID == 0) { // Child process
	write_out = rg->replicas[index].pipefd[1];
	break;
      } else { // Parent Process
	rg->replicas[index].pid = currentPID;
      }
    } else {
      printf("Fork error!\n");
      return -1;
    }
  }

  // TODO: Errors
  return write_out;
}