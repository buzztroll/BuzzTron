PROGS=fun

FUN_CFLAGS=-Wall

all:  $(PROGS)

fun: fun.c chat_io.o chat_timeq.o chat_table.o 
	gcc -I. $(FUN_CFLAGS)  -o fun \
		fun.c chat_io.o chat_timeq.o chat_table.o \
        -lncurses

chat_io.o: chat_io.c chat_io.h
	gcc -I. $(FUN_CFLAGS) -c chat_io.c

chat_timeq.o: chat_timeq.c chat_timeq.h
	gcc -I. $(FUN_CFLAGS) -c chat_timeq.c

chat_table.o: chat_table.c chat_table.h
	gcc -I. $(FUN_CFLAGS) -c chat_table.c


clean:
	rm -f $(PROGS) *~ *.o
