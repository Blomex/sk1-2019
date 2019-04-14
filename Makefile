

.PHONY all:
all: serwer.o klient.o netstore-server netstore-client

netstore-server: serwer.o err.o
	gcc -o netstore-server serwer.o err.o
	
netstore-client: klient.o err.o
	gcc -o netstore-client klient.o err.o

serwer.o: serwer.c err.h
	gcc -c serwer.c

klient.o: klient.c err.h
	gcc -c klient.c
err.o: err.c err.h
	gcc -c err.c
	
.PHONY clean:

clean: 
	rm netstore-server serwer.o klient.o err.o netstore-client
