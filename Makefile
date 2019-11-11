all: wuw_server 
LIBS = -lpthread -levent
SRCDIR = src
SRCS := $(shell find $(SRCDIR) -name "*.c")

wuw_server: 
	gcc -W -Wall $(LIBS) -o $@ ${SRCS}

clean:
	rm wuw_server
