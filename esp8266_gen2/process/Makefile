all : process lighthouse_clocking

process : process.c
	gcc -o $@ $^

lighthouse_clocking : lighthouse_clocking.c
	gcc -o $@ $^


testrec :
	nc 192.168.4.1 10900 | ./process

clean :
	rm -rf *.o *~ lighthouse_clocking process

