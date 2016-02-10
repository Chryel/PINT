include ../directory_paths

SRC=../src/commtypes.c ../src/fd_client.c ../src/controller.c
DEP=../include/*.h ../tas_lib/inc/*.h
INC=-I../include/ -I../tas_lib/inc/
LIB=-lrt -lTASLimited
FLAGS=-O3 -Werror -static
#FLAGS=-g -O0 -Werror -static

all: ArtPot AStar BenchMarker Client CS Empty Filter Logger Mapper Server #Load

%.o: %.c
	gcc -c $(FLAGS) -o $@ $< -I../include/ -I../tas_lib/inc/

ArtPot: art_pot.o $(SRC) $(DEP)
	gcc $(FLAGS) -o ArtPot art_pot.o $(SRC) $(INC) $(LIB) -lm

AStar: a_star.o mapping.c $(SRC) $(DEP) ./inc/*.h
	gcc $(FLAGS) -o AStar a_star.o mapping.c $(SRC) $(INC) $(LIB) -lm

BenchMarker: benchmarker.o ../src/commtypes.c ../src/replicas.c ../voter/vote_buff.c $(DEP)
	gcc $(FLAGS) -o BenchMarker benchmarker.o ../src/commtypes.c ../src/fd_server.c ../src/replicas.c ../voter/vote_buff.c $(INC) $(LIB)

Client:
	gcc client.c -o client

CS: 	cs.o ../src/commtypes.c ../src/replicas.c ../voter/vote_buff.c $(DEP)
	gcc $(FLAGS) -o CS cs.o ../src/commtypes.c ../src/fd_server.c ../src/replicas.c ../voter/vote_buff.c $(INC) $(LIB)

Empty: empty.o $(SRC) $(DEP)
	gcc $(FLAGS) -o Empty empty.o $(SRC) $(INC) $(LIB) 

Filter: filter.o $(SRC) $(DEP)
	gcc $(FLAGS) -o Filter filter.o $(SRC) $(INC) $(LIB) 

Logger: logger.o $(SRC) $(DEP)
	gcc $(FLAGS) -o Logger logger.o $(SRC) $(INC) $(LIB) -lm

Mapper: mapper.o mapping.c $(SRC) $(DEP) ./inc/*.h
	gcc $(FLAGS) -o Mapper mapper.o mapping.c $(SRC) $(INC) $(LIB) -lm

Load: load.o $(SRC) $(DEP)
	gcc -O3 -Werror -L"~/Documents/Senior_Design/code/ccv/lib" -I"~/Documents/Senior_Design/code/ccv/lib" -o Load load.o $(SRC) $(INC) $(LIB) -lccv `cat ~/Documents/Senior_Design/code/ccv/lib/.deps`

GenericEmpty: generic_empty.o $(SRC) $(DEP)
	gcc $(FLAGS) -o Empty generic_empty.o $(SRC) $(INC) $(LIB) 

Server:
	gcc server.c -o server

copy:
	cp ArtPot $(INSTALL_DIR)
	cp AStar $(INSTALL_DIR)
	cp BenchMarker $(INSTALL_DIR)
	cp Empty $(INSTALL_DIR)
	cp Filter $(INSTALL_DIR)
	cp Logger $(INSTALL_DIR)
	cp Mapper $(INSTALL_DIR)
#	cp Load $(INSTALL_DIR)

clean:
	rm -f *.o
	rm -f ArtPot
	rm -f AStar
	rm -f BenchMarker
	rm -f client
	rm -f CS
	rm -f Empty
	rm -f Filter
	rm -f Logger
	rm -f Mapper
	rm -f Load
	rm -f server
