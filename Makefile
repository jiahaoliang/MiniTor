make: proja

proja: proja.o 
	g++ -o proja proja.o -g

proja.o: proja.cpp
	g++ -g -c -Wall proja.cpp

clean:
	rm -f *.o proja
