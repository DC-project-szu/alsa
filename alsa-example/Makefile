OBJS:=open_pcm.o write_pcm.o
CFLAGS:=-g -Wall -lasound

all:$(OBJS)
	gcc $(CFLAGS) -o open_pcm open_pcm.c
	gcc $(CFLAGS) -o write_pcm write_pcm.c

open_pcm.o:open_pcm.c
	gcc -c open_pcm.c

write_pcm.o:write_pcm.c
	gcc -c write_pcm.c


.PHONY:clean
clean:
	rm -rf open_pcm write_pcm $(OBJS)
