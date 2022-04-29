CC = gcc
CFLAGS = -Wall -g
DEPS = encrypt-module.h
OBJ = encrypt-module.o encrypt-drive.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

encrypt:$(OBJ)
	$(CC) -lpthread -o $@ $^ $(CFLAGS)

clean:
	rm -f encrypt encrypt-drive.o encrypt-module.o