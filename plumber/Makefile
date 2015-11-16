default: pb.l pb.y
	bison -d pb.y
	flex pb.l
	cc -g -o plumber ../voter/vote_buff.c ../src/commtypes.c plumbing.c pb.tab.c lex.yy.c -lfl

copy:
	cp plumber ../../Stage/experiments/

clean:
	rm -rf plumber
	rm -rf lex.yy.c
	rm -rf pb.tab.c
	rm -rf pb.tab.h

