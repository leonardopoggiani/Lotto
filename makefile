all: lotto_client lotto_server

lotto_client: lotto_client.o
	gcc -g -Wall lotto_client.o -o lotto_client 

lotto_server: lotto_server.o
	gcc -g -Wall lotto_server.o -o lotto_server

clean:
	rm *o lotto_server lotto_client

	
