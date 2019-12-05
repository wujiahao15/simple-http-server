all: wuw_server 

LIBS = -levent -lssl -lcrypto -levent_openssl
LDFLAGS=-L/usr/local/opt/openssl/lib
CPPFLAGS=-I/usr/local/opt/openssl/include

SRCDIR = src
SRCS := $(shell find $(SRCDIR) -name "*.c")

wuw_server: 
	gcc -W -g -Wall $(LDFLAGS) $(CPPFLAGS) $(LIBS) -o $@ ${SRCS}

clean:
	rm wuw_server
