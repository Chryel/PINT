%.o: %.c
	gcc -c -o $@ $<

inject_error: register_util.o print_registers.h
	gcc -o inject_error register_util.o

copy:
	cp inject_error ../../Stage/experiments
	cp injector.py ../../Stage/experiments

clean:
	rm -f *.o
	rm -f inject_error


