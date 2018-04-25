all : test_system

CFLAGS:=-I../user -I. -g

test_system : test_system.c ../user/lighthouse_decode.c ../user/lighthouse_i2s_lut.c
	gcc -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean :
	rm -rf *.o *~ test_system
