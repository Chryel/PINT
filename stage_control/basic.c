/*
 * Connects to a position2d device (number 1) at the specified IP.
 */

#include <libplayerc/playerc.h>

// from: http://playerstage.sourceforge.net/doc/Player-svn/player/group__libplayerc__example.html
int main(int argc, const char **argv) {
  int i;
  playerc_client_t *client;
  playerc_client_t *sim_client;
  playerc_position2d_t *position2d;
  playerc_simulation_t *simulation;

  if (argc < 2) {
    puts("Usage: basic <simulator_ip_address>");
    puts("Possible simulator ips:\n\tLaptop: 161.253.66.53\n\tScotty's: 192.168.100.1\n");
    return 0;
  }

  // Create client and connect
  client = playerc_client_create(0, "127.0.0.1", 6666); // I start at 6666
  if (0 != playerc_client_connect(client)) {
    return -1;
  }

  // Create client for the sim commands and connect
  sim_client = playerc_client_create(0, argv[1], 6665);
  if (0 != playerc_client_connect(sim_client)) {
    return -1;
  }

  // Create and subscribe to position2d device
  position2d = playerc_position2d_create(client, 1);
  if (playerc_position2d_subscribe(position2d, PLAYER_OPEN_MODE)) {
    return -1;
  }

  // Create and subscribe to sim device (to reset position)
  simulation = playerc_simulation_create(sim_client, 0);
  if (playerc_simulation_subscribe(simulation, PLAYER_OPEN_MODE)) {
    return -1;
  }

  // Reset the robot position
  playerc_simulation_set_pose2d(simulation, "hank", -7.0, -7.0, 0.0);

  // Make robot move
  //  playerc_position2d_enable(position2d, 1);
  //  playerc_position2d_set_cmd_pose(position2d, 7.0, 7.0, 0.0, 1);
  playerc_position2d_set_cmd_pose(position2d, 4.5, 4.5, 0.0, 1);

  while(1) {
    sleep(1); // blah
  }

  // Shutdown
  playerc_position2d_unsubscribe(position2d);
  playerc_position2d_destroy(position2d);
  playerc_client_disconnect(client);
  playerc_client_destroy(client);

  return 0;
}

