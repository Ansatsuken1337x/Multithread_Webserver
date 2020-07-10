all: multithread_webserver
 
multithread_webserver: main.o 
    gcc -o multithread_webserver main.o 
 
main.o: main.c
    gcc -o main.o main.c -c -W -Wall -lpthread -O9 -g
 
clean:
    rm -rf *.o *~ multithread_webserver