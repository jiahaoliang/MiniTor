make: projb

projb: proja.o sample_tunnel.o
	g++ -o proja proja.o sample_tunnel.o -g

proja.o: proja.cpp sample_tunnel.h
	g++ -g -c -Wall proja.cpp
	
sample_tunnel.o: sample_tunnel.h sample_tunnel.c
	g++ -g -c -Wall sample_tunnel.c

clean:
	rm -f *.o projb *.out
