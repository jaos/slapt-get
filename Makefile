PROGNAME=jaospkg
CC=gcc
CURLFLAGS=`curl-config --libs`
CFLAGS=-W -Werror -Wall -O2 -ansi -pedantic -Iinclude
DEBUGFLAGS=-g
OBJS=src/configuration.o src/package.o src/curl.o src/action.o src/main.o
RCDEST=/etc/jaospkgrc
RCSOURCE=example.jaospkgrc
SBINDIR=/sbin/

default: all

all: $(PROGNAME)

$(OBJS): 

$(PROGNAME): $(OBJS)
	$(CC) $(CFLAGS) $(CURLFLAGS) -o $(PROGNAME) $(OBJS)

$(PROGNAME)-debug: $(OBJS)
	$(CC) $(CFLAGS) $(CURLFLAGS) $(DEBUGFLAGS) -o $(PROGNAME) $(OBJS)

install: jaospkg
	install $(PROGNAME) $(SBINDIR)
	install --mode=0644 -b $(RCSOURCE) $(RCDEST)

uninstall:
	-rm /sbin/$(PROGNAME)
	-@echo leaving $(RCDEST)

clean:
	-if [ -f $(PROGNAME) ]; then rm $(PROGNAME);fi
	-rm src/*.o

