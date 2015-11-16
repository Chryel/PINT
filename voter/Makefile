LIB=-lrt -lTASLimited
INC=-I../include/ -I../tas_lib/inc/
FLAGS=-O3 -Werror -static

all: VoterD VoterD_R

%.o: %.c ../include/*.h ../tas_lib/inc/*.h
	gcc -c $(FLAGS) -o $@ $< $(INC)

VoterD: voterd.o vote_buff.c ../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../src/fd_client.c
	gcc $(FLAGS) -o VoterD voterd.o vote_buff.c ../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../src/fd_client.c $(INC) $(LIB)

VoterD_R: voterd_r.o vote_buff.c ../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../src/fd_client.c
	gcc $(FLAGS) -o VoterD_R voterd_r.o vote_buff.c ../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../src/fd_client.c $(INC) $(LIB)

test: test_buff.o vote_buff.c
	gcc -g -o testBuff test_buff.o vote_buff.c $(INC) $(LIB)

copy:
	cp VoterD ../../Stage/experiments/
	cp VoterD_R ../../Stage/experiments/

clean:
	rm -f *.o
	rm -f VoterD
	rm -f VoterD_R
	rm -f testBuff
