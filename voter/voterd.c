/*
 * Voter that is able to start and connect to other voters, and maybe
 * even be generic
 *
 * Author - James Marshall
 */

#include "../include/controller.h"

#include <assert.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <linux/prctl.h>
#include <time.h>

#include "../include/replicas.h"
 
#define REP_MAX 3
#define PERIOD_NSEC 120000 // Max time for voting in nanoseconds (120 micro seconds)
#define VOTER_PRIO_OFFSET 5 // Replicas run with a -5 offset

long voting_timeout;
int timer_start_index;
int timer_stop_index;
bool timer_started;
timestamp_t watchdog;

// Replica related data
struct replica replicas[REP_MAX];

// TAS Stuff
int voter_priority;

// FD server
struct server_data sd;

replication_t rep_type;
int rep_count;
char* controller_name;
// pipes to external components (not replicas)
int pipe_count = 0;
struct typed_pipe ext_pipes[PIPE_LIMIT];

// Functions!
int initVoterD(void);
int parseArgs(int argc, const char **argv);
void doOneUpdate(void);
void processData(struct typed_pipe *pipe, int pipe_index);
void resetVotingState(int pipe_num);
void initVotingState(void);
void sendPipe(int pipe_num, int replica_num);
void stealBuffers(int rep_num, char **buffer, int *buff_count);
void returnBuffers(int rep_num, char **buffer, int *buff_count);
void checkSDC(int pipe_num);
void processFromRep(int replica_num, int pipe_num);
void writeBuffer(int fd_out, char* buffer, int buff_count);

void restart_prep(int restartee, int restarter) {
  #ifdef TIME_RESTART_REPLICA
    timestamp_t start_restart = generate_timestamp();
  #endif // TIME_RESTART_REPLICA
  
  int i;
  char **restarter_buffer = (char **)malloc(sizeof(char *) * PIPE_LIMIT);
  if (restarter_buffer == NULL) {
    perror("Voter failed to malloc memory");
  }
  for (i = 0; i < PIPE_LIMIT; i++) {
    restarter_buffer[i] = (char *)malloc(sizeof(char) * MAX_PIPE_BUFF);
    if (restarter_buffer[i] == NULL) {
      perror("Voter failed to allocat memory");
    }
  }
  int restarter_buff_count[PIPE_LIMIT] = {0};

  // Steal the buffers from healthy reps. This stops them from processing mid restart
  stealBuffers(restarter, restarter_buffer, restarter_buff_count);

  // reset timer
  timer_started = false;
  restartReplica(replicas, rep_count, &sd, ext_pipes, restarter, restartee, voter_priority - VOTER_PRIO_OFFSET);

  for (i = 0; i < replicas[restarter].pipe_count; i++) {
    if (replicas[restarter].vot_pipes[i].fd_in != 0) { // && reps[restarter].voted[i] > 0) { // was causing problems with the timer
      replicas[restartee].voted[i] = replicas[restarter].voted[i];
      memcpy(replicas[restartee].vot_pipes[i].buffer, replicas[restarter].vot_pipes[i].buffer, replicas[restarter].vot_pipes[i].buff_count);
      replicas[restartee].vot_pipes[i].buff_count = replicas[restarter].vot_pipes[i].buff_count;
      sendPipe(i, restarter);
    }
  }

  // Give the buffers back
  returnBuffers(restartee, restarter_buffer, restarter_buff_count);
  returnBuffers(restarter, restarter_buffer, restarter_buff_count);
  // free the buffers
  for (i = 0; i < PIPE_LIMIT; i++) {
    free(restarter_buffer[i]);
  }
  free(restarter_buffer);

  #ifdef TIME_RESTART_REPLICA
    timestamp_t end_restart = generate_timestamp();
    printf("Restart time elapsed (%lld)\n", end_restart - start_restart);
  #endif // TIME_RESTART_REPLICA

  return;
}

void voterRestartHandler(void) {
  // Timer went off, so the timer_stop_index is the pipe which is awaiting a rep
  int p_index;
  debug_print("Caught Exec / Control loop error\n");

  switch (rep_type) {
    case SMR: {
      // Need to cold restart the replica
      cleanupReplica(replicas, 0);

      startReplicas(replicas, rep_count, &sd, controller_name, ext_pipes, pipe_count, voter_priority - VOTER_PRIO_OFFSET);
      
      // Resend last data
      for (p_index = 0; p_index < pipe_count; p_index++) {
        int read_fd = ext_pipes[p_index].fd_in;
        if (read_fd != 0) {
          processData(&(ext_pipes[p_index]), p_index);    
        }
      }

      break;
    }
    case DMR: {
    }
    case TMR: {
      // The failed rep should be the one behind on the timer pipe
      int restartee = behindRep(replicas, rep_count, timer_stop_index);
      int restarter = (restartee + (rep_count - 1)) % rep_count;
      restart_prep(restartee, restarter);
      break;
    }

    return;
  }
}

// Steal the buffers from a single replica
// buff_count and buffer should already be alocated.
void stealBuffers(int rep_num, char **buffer, int *buff_count) {
  int i = 0;

  struct timeval select_timeout;
  fd_set select_set;
  select_timeout.tv_sec = 0;
  select_timeout.tv_usec = 0;

  FD_ZERO(&select_set);
  for (i = 0; i < replicas[rep_num].pipe_count; i++) {
    if (replicas[rep_num].voter_rep_in_copy[i] != 0) {
      FD_SET(replicas[rep_num].voter_rep_in_copy[i], &select_set);
    }
  }

  int retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);
  if (retval > 0) { // Copy buffers
    for (i = 0; i < replicas[rep_num].pipe_count; i++) {
      if (FD_ISSET(replicas[rep_num].voter_rep_in_copy[i], &select_set)) {
        buff_count[i] = read(replicas[rep_num].voter_rep_in_copy[i], buffer[i], MAX_PIPE_BUFF);
        if (buff_count[i] < 0) {
          perror("Voter error stealing pipe");
        }
      }
    }
  }  
}

// replace buffers in a replica. Does NOT free the buffer (so the same one can be copied again)
void returnBuffers(int rep_num, char **buffer, int *buff_count) {  
  int i = 0;
  for (i = 0; i < replicas[rep_num].pipe_count; i++) {
    if (buff_count[i] > 0) {
      writeBuffer(replicas[rep_num].vot_pipes[i].fd_out, buffer[i], buff_count[i]);
    }
  }
}

bool checkSync(void) {
  int r_index, p_index;
  bool nsync = true;

  for (r_index = 0; r_index < rep_count; r_index++) {
    int votes = replicas[r_index].voted[0];
    for (p_index = 0; p_index < pipe_count; p_index++) {
      if (votes != replicas[r_index].voted[p_index]) {
        nsync = false;
      }
    }
  }
  return nsync;
}

void doOneUpdate(void) {
  int p_index, r_index;
  int retval = 0;

  struct timeval select_timeout;
  fd_set select_set;

  //waitpid(-1, NULL, WNOHANG); // Seems to take a while for to clean up zombies

  select_timeout.tv_sec = 0;
  select_timeout.tv_usec = 50000;

  if (timer_started) {
    timestamp_t current = generate_timestamp();
    long remaining = voting_timeout - ((current - watchdog) / 3.092);
    if (remaining > 0) {
      select_timeout.tv_sec = 0;
      select_timeout.tv_usec = remaining / 1000;
    } else {
      // printf("Restart handler called, %ld late\n", remaining);
      voterRestartHandler();
    }
  }

  // See if any of the read pipes have anything
  FD_ZERO(&select_set);
  // Check external in pipes
  // Hmm... only if the controllers are ready for it?
  bool check_inputs = false;
  if (voter_priority < 5) {
    if (checkSync() | !timer_started) {
      check_inputs = true;
    }
  } else {
    check_inputs = true;
  }

  if (check_inputs) {
    for (p_index = 0; p_index < pipe_count; p_index++) {
      if (ext_pipes[p_index].fd_in != 0) {
        int e_pipe_fd = ext_pipes[p_index].fd_in;
        FD_SET(e_pipe_fd, &select_set);
      }
    }
  }

  // Check pipes from replicas
  for (r_index = 0; r_index < rep_count; r_index++) {
    for (p_index = 0; p_index < replicas[r_index].pipe_count; p_index++) {
      int rep_pipe_fd = replicas[r_index].vot_pipes[p_index].fd_in;
      if (rep_pipe_fd != 0) {
        FD_SET(rep_pipe_fd, &select_set);      
      }
    }
  }

  // This will wait at least timeout until return. Returns earlier if something has data.
  retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);

  if (retval > 0) {    
    // Check for data from external sources
    for (p_index = 0; p_index < pipe_count; p_index++) {
      int read_fd = ext_pipes[p_index].fd_in;
      if (read_fd != 0) {
        if (FD_ISSET(read_fd, &select_set)) {
          if (voter_priority < 5 && !checkSync()) {
            // non-RT controller is now lagging behind.
            timer_started = true;
            watchdog = generate_timestamp();
          }
          ext_pipes[p_index].buff_count = TEMP_FAILURE_RETRY(read(read_fd, ext_pipes[p_index].buffer, MAX_PIPE_BUFF));
          if (ext_pipes[p_index].buff_count > 0) { // TODO: read may still have been interrupted
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

    // Check all replicas for data
    for (r_index = 0; r_index < rep_count; r_index++) {
      for (p_index = 0; p_index < replicas[r_index].pipe_count; p_index++) {
        struct typed_pipe* curr_pipe = &(replicas[r_index].vot_pipes[p_index]);
        if (curr_pipe->fd_in !=0) {
          if (FD_ISSET(curr_pipe->fd_in, &select_set)) {
            processFromRep(r_index, p_index);
          }
        }
      }
    }
  }
}

void writeBuffer(int fd_out, char* buffer, int buff_count) {
  int retval = TEMP_FAILURE_RETRY(write(fd_out, buffer, buff_count));
  if (retval == buff_count) {
    // success, do nothing
  } else if (retval > 0) { // TODO: resume write? 
    printf("Voter for %s, pipe %d, bytes written: %d\texpected: %d\n", controller_name, fd_out, retval, buff_count);
    perror("Voter wrote partial message");
  } else if (retval < 0) {
    printf("Voter for %s failed write fd: %d\n", controller_name, fd_out);
    perror("Voter write");
  } else {
    printf("Voter wrote == 0 for %s fd: %d\n", controller_name, fd_out);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Process data
void processData(struct typed_pipe *pipe, int pipe_index) {
  int r_index;
  if (pipe_index == timer_start_index) {
    if (!timer_started) {
      timer_started = true;
      watchdog = generate_timestamp();
    }
  }

  balanceReps(replicas, rep_count, voter_priority - VOTER_PRIO_OFFSET);

  for (r_index = 0; r_index < rep_count; r_index++) {
    writeBuffer(replicas[r_index].vot_pipes[pipe_index].fd_out, pipe->buffer, pipe->buff_count);
  }
}

////////////////////////////////////////////////////////////////////////////////
// reset / init voting state
void resetVotingState(int pipe_num) {
  int r_index;
  for (r_index = 0; r_index < rep_count; r_index++) {
    replicas[r_index].voted[pipe_num]--; // = 0;
  }
}

void initVotingState(void) {
  int p_index;
  int r_index;
  for (p_index = 0; p_index < pipe_count; p_index++) {
    for (r_index = 0; r_index < rep_count; r_index++) {
      replicas[r_index].voted[p_index] = 0;
    }
  }
}

bool allReporting(int pipe_num) {
  bool all_reporting = true;
  int r_index;
  for (r_index = 0; r_index < rep_count; r_index++) {
    // Check that all have reported
    all_reporting = all_reporting && 
      (replicas[r_index].voted[pipe_num] == replicas[(r_index + 1) % rep_count].voted[pipe_num]);
  }

  return all_reporting;
}

void sendPipe(int pipe_num, int replica_num) {
  if (!allReporting(pipe_num)) {
    return;
  }
  
  if (pipe_num == timer_stop_index) {  
    // reset the timer
    timer_started = false;
  }

  writeBuffer(ext_pipes[pipe_num].fd_out, replicas[replica_num].vot_pipes[pipe_num].buffer, replicas[replica_num].vot_pipes[pipe_num].buff_count);
  
  resetVotingState(pipe_num);
}

void checkSDC(int pipe_num) {
  int r_index;

  if (!allReporting(pipe_num)) {
    return;
  }

  switch (rep_type) {
    case SMR: 
      // Only one rep, so pretty much have to trust it
      sendPipe(pipe_num, 0);
      return;
    case DMR:
      // Can detect, and check what to do
      if (memcmp(replicas[0].vot_pipes[pipe_num].buffer,
                 replicas[1].vot_pipes[pipe_num].buffer,
                 replicas[0].vot_pipes[pipe_num].buff_count) != 0) {
        printf("Voting disagreement: caught SDC in DMR but can't do anything about it.\n");
      }

      sendPipe(pipe_num, 0);
      return;
    case TMR:
      // Send the solution that at least two agree on
      // TODO: What if buff_count is off?
      for (r_index = 0; r_index < rep_count; r_index++) {
        if (memcmp(replicas[r_index].vot_pipes[pipe_num].buffer,
                   replicas[(r_index+1) % rep_count].vot_pipes[pipe_num].buffer,
                   replicas[r_index].vot_pipes[pipe_num].buff_count) == 0) {

          // If the third doesn't agree, it should be restarted.
          if (memcmp(replicas[r_index].vot_pipes[pipe_num].buffer,
                     replicas[(r_index + 2) % rep_count].vot_pipes[pipe_num].buffer,
                     replicas[r_index].vot_pipes[pipe_num].buff_count) != 0) {
            
            debug_print("Caught SDC\n");

            int restartee = (r_index + 2) % rep_count;
            restart_prep(restartee, r_index);
          } else {
            // If all agree, send and be happy. Otherwise the send is done as part of the restart process
            sendPipe(pipe_num, r_index);
          }
          return;
        } 
      }

      printf("VoterD: TMR no two replicas agreed.\n");
  }
}

// New message came in, but already have a message
void emergencyWrite(int pipe_num, int replica_num) {
  switch (rep_type) {
  case SMR: 
    // Wut? This makes no sense
    printf("Voter emergencyWrite error: SMR.");
    return;
  case DMR:
    // Just send it.
    writeBuffer(ext_pipes[pipe_num].fd_out, replicas[replica_num].vot_pipes[pipe_num].buffer, replicas[replica_num].vot_pipes[pipe_num].buff_count);
    resetVotingState(pipe_num);
    return;
  case TMR:
    // check if match? or just send it.
    writeBuffer(ext_pipes[pipe_num].fd_out, replicas[replica_num].vot_pipes[pipe_num].buffer, replicas[replica_num].vot_pipes[pipe_num].buff_count);
    resetVotingState(pipe_num);
    return;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Process output from replica; vote on it
void processFromRep(int replica_num, int pipe_num) {
  if (replicas[replica_num].voted[pipe_num] >= 1) { // Already have a pending message... will lose data if read is done
    emergencyWrite(pipe_num, replica_num);
  }
  struct typed_pipe* curr_pipe = &(replicas[replica_num].vot_pipes[pipe_num]);
  curr_pipe->buff_count = TEMP_FAILURE_RETRY(read(curr_pipe->fd_in, curr_pipe->buffer, MAX_PIPE_BUFF));

  // TODO: Read may have been interrupted
  if (curr_pipe->buff_count > 0) {
    replicas[replica_num].voted[pipe_num]++;

    balanceReps(replicas, rep_count, voter_priority - VOTER_PRIO_OFFSET);
    
    if (replicas[replica_num].voted[pipe_num] > 1) {
      // Happens when a process has died.
      // printf("Run-away lag detected: %s pipe - %d - rep 0, 1, 2: %d, %d, %d\n", controller_name, pipe_num, replicas[0].voted[pipe_num], replicas[1].voted[pipe_num], replicas[2].voted[pipe_num]);
    }

    checkSDC(pipe_num);
  } else if (curr_pipe->buff_count < 0) {
    printf("Voter - Controller %s, rep %d, pipe %d\n", controller_name, replica_num, pipe_num);
    perror("Voter - read problem on internal pipe");
  } else {
    printf("Voter - Controller %s, rep %d, pipe %d\n", controller_name, replica_num, pipe_num);
    perror("Voter - read == 0 on internal pipe");
  }
}

////////////////////////////////////////////////////////////////////////////////
// Set up the device.  Return 0 if things go well, and -1 otherwise.
int initVoterD(void) {
  struct sigevent sev;
  sigset_t mask;

  InitTAS(DEFAULT_CPU, voter_priority);

  EveryTAS();

  // Setup fd server
  createFDS(&sd, controller_name);

  initVotingState();

  startReplicas(replicas, rep_count, &sd, controller_name, ext_pipes, pipe_count, voter_priority - VOTER_PRIO_OFFSET);
  
  return 0;
}

int parseArgs(int argc, const char **argv) {
  int i;
  int required_args = 5; // voter name, controller name, rep_type, timeout and priority
  controller_name = (char*) (argv[1]);
  rep_type = reptypeToEnum((char*)(argv[2]));
  rep_count = rep_type;
  voting_timeout = atoi(argv[3]);
  voter_priority = atoi(argv[4]);
  if (voting_timeout == 0) {
    voting_timeout = PERIOD_NSEC;
  }

  if (argc <= required_args) { // In testing mode // TODO: clear out after testing
    pid_t currentPID = getpid();
    //pipe_count = 4;  // 4 is the only controller specific bit here... and ArtPotTest
    //connectRecvFDS(currentPID, ext_pipes, pipe_count, "ArtPotTest");
    pipe_count = 2;  // 4 is the only controller specific bit here... and ArtPotTest
    connectRecvFDS(currentPID, ext_pipes, pipe_count, "EmptyTest");
    timer_start_index = 0;
    timer_stop_index = 1;
        // puts("Usage: VoterD <controller_name> <rep_type> <timeout> <priority> <message_type:fd_in:fd_out> <...>");
    // return -1;
  } else {
    for (i = 0; (i < argc - required_args && i < PIPE_LIMIT); i++) {
      deserializePipe(argv[i + required_args], &ext_pipes[pipe_count]);
      pipe_count++;
    }
    if (pipe_count >= PIPE_LIMIT) {
      printf("VoterD: Raise pipe limit.\n");
    }
  
    // Need to have a similar setup to associate vote pipe with input?
    for (i = 0; i < pipe_count; i++) {
      if (ext_pipes[i].timed) {
        if (ext_pipes[i].fd_in != 0) {
          timer_start_index = i;
        } else {
          timer_stop_index = i;
        }
      }
    }
  }

  return 0;
}

int main(int argc, const char **argv) {
  if (parseArgs(argc, argv) < 0) {
    puts("ERROR: failure parsing args.");
    return -1;
  }

  if (initVoterD() < 0) {
    puts("ERROR: failure in setup function.");
    return -1;
  }

  while(1) {
    doOneUpdate();
  }

  return 0;
}