all: wuw_server 

LIBS = -levent -lssl -lcrypto -levent_openssl
LDFLAGS=-L/usr/local/lib
INCS=-I/usr/local/include

SRCDIR = src
SRCS := $(shell find $(SRCDIR) -name "*.c")

wuw_server: 
	gcc ${SRCS} -Wall $(LDFLAGS) $(INCS) $(LIBS) -o $@ 
clean:
	rm wuw_server
