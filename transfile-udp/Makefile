OBJS := trans_ser.o trans_cli.o
CFLAGS := -g -Wall

all : $(OBJS)
	gcc $(CFLAGS) -o trans_ser trans_ser.o
	gcc $(CFLAGS) -o trans_cli trans_cli.o


trans_ser.o : trans_ser.c
	gcc -c trans_ser.c

trans_cli.o : trans_cli.c
	gcc -c trans_cli.c


.PHONY:clean
clean:
	rm -rf trans_cli trans_ser $(OBJS)
