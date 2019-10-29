all: http_server client
LIBS = -lpthread #-lsocket#-lpthread #
http_server: http_server.c
	# gcc -g -W -Wall -o $@ $<
	gcc -g -W -Wall $(LIBS) -o $@ $<

client: client.c
	gcc -W -Wall -o $@ $<
clean:
	rm http_server client
