OPENSSL_INC = -I/opt/homebrew/opt/openssl@3/include
OPENSSL_LIB = -L/opt/homebrew/opt/openssl@3/lib

CFLAGS = $(OPENSSL_INC) -Wall -g
LDFLAGS = $(OPENSSL_LIB) -lssl -lcrypto

all:
	gcc $(CFLAGS) *.c mail/*.c -o main $(LDFLAGS)

run:
	./main

clean:
	rm -f main *.o
