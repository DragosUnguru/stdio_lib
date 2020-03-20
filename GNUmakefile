CC = gcc
CFLAGS = -g -Wall -Wextra -std=c89 -fPIC

build: so_stdio.o stdio_internal.o
	$(CC) -shared $^ -o libso_stdio.so

so_stdio.o: so_stdio.c so_stdio.h
	$(CC) $(CFLAGS) -c $<

stdio_internal.o: stdio_internal.c stdio_internal.h
	$(CC) $(CFLAGS) -c $<

.PHONY clean:
clean: libso_stdio.so
	rm *.o libso_stdio.so