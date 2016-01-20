all: regular

regular: libs
	make -C ./controllers
	make -C ./voter
	make -C ./stage_control
	make -C ./plumber
#	make -C ./fault_injection
	make -C ./test

libs:
	make -C ./tas_lib
#	make -C ./player_translator_driver/build

install: regular
	make -C ./controllers copy
	make -C ./voter copy
	make -C ./plumber copy
#	make -C ./fault_injection copy

clean:
	make -C ./controllers clean
	make -C ./voter clean
	make -C ./stage_control clean
	make -C ./plumber clean
#	make -C ./fault_injection clean
	make -C ./test clean
