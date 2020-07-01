soktfile=$(wildcard *.socket)
src=$(wildcard *.c)
obj=$(patsubst %.c, %.o, $(src))
serv_obj=server.o packet.o
clie_obj=client.o packet.o menu.o
target=server client
CFLAGS=-Wall -lls -levent -lpthread

all:$(target)

server:$(serv_obj)
	$(CC) $^ -o $@ $(CFLAGS)
	
client:$(clie_obj)
	$(CC) $^ -o $@ $(CFLAGS)

server.o:server.c packet.c packet.h
	$(CC) -c $< -o $@ $(CFLAGS)

client.o:client.c packet.c packet.h menu.c menu.h
	$(CC) -c $< -o $@ $(CFLAGS)

packet.o:packet.c packet.h
	$(CC) -c $< -o $@ $(CFLAGS)

menu.o:menu.c menu.h
	$(CC) -c $< -o $@ $(CFLAGS)

#%.o:%.c %.h
#	$(CC) -c $< -o $@ $(CFLAGS)

.PHONY:clean
clean:
	rm -rf $(target) $(obj) $(sockfile)


