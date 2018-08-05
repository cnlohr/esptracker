all : fx3process fx3process2 data3demodulate_plotBASIC

fx3process : fx3process.c
	gcc -o $@ $^ -Os

fx3process2 : fx3process2.c
	gcc -o $@ $^ -Os -lfftw

data3demodulate_plotBASIC : data3demodulate_plotBASIC.c
	gcc -o $@ $^ -Os

clean :
	rm -rf fx3process fx3process2 data3demodulate_plotBASIC
