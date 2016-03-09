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

// restart timer fd
char timeout_byte[1] = {'*'};
char heartbeat_byte[1] = {'h'};
int timeout_fd[2];
int heartbeat_fd[2];
timer_t timerid;
struct itimerspec its;

int initRIS(){
	
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

int serverComm(int count, char *strings[]){
        struct sockaddr_in addr;
        struct sigaction act;
        int bytes;
        char line[100];

        if(count != 3){
                printf("Parameters: %s <address> <port>\n", strings[0]);
                exit(0);

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
}

int main(){
	pid_t pid = 0;

	//Arm Timer
        its.it_interval.tv_sec = 0;
        its.it_interval.tv_nsec = 0;
        its.it_value.tv_sec = 2;
        its.it_value.tv_nsec = 0;
	
	if(initRIS() < 0){
		puts("ERROR: initRIS failed.\n");
		return -1;
	}

	pid fork();
	if(pid >= 0){
		if(pid = 0){
			//Launching simulator
		}
		else{
			serverComm(count, strings);
			
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
				}
			}		
		}
	} else{
		printf("RIS - Error while forking");
		return -1;
	}
}
