/*
 * Builds a map of obstacles
 *
 * James Marshall
 */

#include <assert.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "../include/taslimited.h"
#include "../include/commtypes.h"
#include "../include/statstime.h"
#include "../include/fd_client.h"

#define MAP_SIZE 16
#define OFFSET_X  8
#define OFFSET_Y  8
#define GRID_NUM 64

#define RANGE_COUNT 16

struct typed_pipe pipes[2];

FILE * out_file;

void enterLoop();
int initReplica();

struct point_i {
  int x;
  int y;
};

struct point_d {
  double x;
  double y;
};

struct point_i* current_pose;
// Count to 3 method worked great before
bool obstacle_map[GRID_NUM][GRID_NUM];

void restartHandler(int signo) {
  pid_t currentPID = 0;
  // fork
  currentPID = fork();

  if (currentPID >= 0) { // Successful fork
    if (currentPID == 0) { // Child process
      // child sets new id, recreates connects, loops
      initReplica();
      // Get own pid, send to voter
      currentPID = getpid();
      connectRecvFDS(currentPID, pipes, 2, "Mapper");
      
      // unblock the signal
      sigset_t signal_set;
      sigemptyset(&signal_set);
      sigaddset(&signal_set, SIGUSR1);
      sigprocmask(SIG_UNBLOCK, &signal_set, NULL);

      enterLoop(); // return to normal
    } else {   // Parent just returns
      return;
    }
  } else {
    printf("Fork error!\n");
    return;
  }
}

int parseArgs(int argc, const char **argv) {
  // TODO: error checking
  if (argc < 2) { // Must request fds
    printf("Mapper usage message.\n");
  } else {
    deserializePipe(argv[1], &pipes[0]);
    deserializePipe(argv[2], &pipes[1]);
  }

  return 0;
}

// TODO: Should probably separate this out correctly
// Basically the init function
int initReplica() {
  if (signal(SIGUSR1, restartHandler) == SIG_ERR) {
    puts("Failed to register the restart handler");
    return -1;
  }
  return 0;
}

struct point_i* gridify(struct point_d* p) {
  struct point_i* new_point = (struct point_i*) malloc(sizeof(struct point_i));
  int x, y;
  double interval = MAP_SIZE / (double)GRID_NUM;
  x = (int)((p->x + OFFSET_X) / interval);
  y = (int)((p->y + OFFSET_Y) / interval);

  // Account for edge of map
  if (x == GRID_NUM) {
    x--;
  }
  if (y == GRID_NUM) {
    y--;
  }
  new_point->x = x;
  new_point->y = y;

  return new_point;
}

// return true if something changed
bool addObstacle(struct point_i* obs) {
  if (obstacle_map[obs->x][obs->y]) {
    // obstacle already there, return false (no changes)
    free(obs);
    return false;
  } else {
    obstacle_map[obs->x][obs->y] = true;
    commSendMapUpdate(pipes[1], obs->x, obs->y, current_pose->x, current_pose->y);
    free(obs);
    return true;
  }
}

void updateMap(struct comm_range_pose_data * data) {
  double x_pose, y_pose, theta_pose;
  // Read pose
  struct point_d pose;
  pose.x = data->pose[INDEX_X];
  pose.y = data->pose[INDEX_Y];
  theta_pose = data->pose[INDEX_A];

  bool changed = false;

  free(current_pose);
  current_pose = gridify(&pose);

  // Convert ranges absolute positions
  for (int i = 0; i < RANGE_COUNT; i++) {
    struct point_d obstacle_l, obstacle_g;

    // obstacle location relative to the robot
    double tao = (2 * M_PI * i) / RANGE_COUNT;
    obstacle_l.x = data->ranges[i] * cos(tao);
    obstacle_l.y = data->ranges[i] * sin(tao);
    
    // obstacle location in global coords
    obstacle_g.x = obstacle_l.x * cos(theta_pose) - obstacle_l.y * sin(theta_pose);
    obstacle_g.x += x_pose;
    obstacle_g.y = obstacle_l.x * sin(theta_pose) + obstacle_l.y * cos(theta_pose);
    obstacle_g.y += y_pose;

    changed = addObstacle(gridify(&obstacle_g)) || changed;
  }

  // send new map out
  if (changed) {
    /*
    for (int i = GRID_NUM - 1; i >= 0; i--) {
      for (int j = 0; j < GRID_NUM; j++) {
        if (obstacle_map[j][i]) {
          fprintf(out_file, "X");
        } else {
          fprintf(out_file, ".");
        }
      }
      fprintf(out_file, "\n");
    }
    fprintf(out_file, "\n");
    */
  }
}

void enterLoop() {
  int read_ret;
  struct comm_range_pose_data recv_msg;

  // For now I'm goint to write out everything to a file
  out_file = fopen("map_output.txt", "w");
  if (out_file == NULL)
  {
    printf("Error opening file!\n");
    exit(1);
  }

  for (int i = 0; i < GRID_NUM; i++) {
    for (int j = 0; j < GRID_NUM; j++) {
      obstacle_map[i][j] = false;
    }
  }
 
  while(1) {
    // Blocking, but that's okay with me
    read_ret = read(pipes[0].fd_in, &recv_msg, sizeof(struct comm_range_pose_data));
    if (read_ret > 0) {
      // TODO: Error checking
      updateMap(&recv_msg);
    } else if (read_ret == -1) {
      perror("Blocking, eh?");
    } else {
      puts("Mapper read_ret == 0?");
    }
  }
}

int main(int argc, const char **argv) {
  if (parseArgs(argc, argv) < 0) {
    puts("ERROR: failure parsing args.");
    return -1;
  }

  if (initReplica() < 0) {
    puts("ERROR: failure in setup function.");
    return -1;
  }

  enterLoop();

  return 0;
}

