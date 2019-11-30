all: wuw_server 
LIBS = -lpthread -levent -lssl
LDFLAGS=-L/usr/local/opt/openssl@1.1/lib
INCLUDE=-I/usr/local/opt/openssl@1.1/include
SRCDIR = src
SRCS := $(shell find $(SRCDIR) -name "*.c")

wuw_server: 
	gcc -W -Wall $(LDFLAGS) $(LIBS) ${INCLUDE} -o $@ ${SRCS}

clean:
	rm wuw_server
