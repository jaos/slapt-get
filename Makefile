PROGNAME=jaospkg
CC=gcc
CURLFLAGS=`curl-config --libs`
#CFLAGS=-Wall -O2 -pedantic -Iinclude
CFLAGS=-W -Werror -Wall -O2 -ansi -pedantic -Iinclude
DEBUGFLAGS=-g
SOURCEFILES= src/configuration.c src/package.c src/curl.c src/action.c src/main.c
RCDEST=/etc/jaospkgrc
RCSOURCE=example.jaospkgrc
SBINDIR=/sbin/

default: $(PROGNAME)

all: $(PROGNAME)

$(PROGNAME):
	$(CC) $(CFLAGS) $(CURLFLAGS) -o $(PROGNAME) $(SOURCEFILES) 

$(PROGNAME)-debug:
	$(CC) $(CFLAGS) $(CURLFLAGS) $(DEBUGFLAGS) -o $(PROGNAME) $(SOURCEFILES) 

install:
	install $(PROGNAME) $(SBINDIR)
	install --mode=0644 -b $(RCSOURCE) $(RCDEST)

uninstall:
	rm /sbin/$(PROGNAME)

clean:
	-@if [ -f $(PROGNAME) ]; then rm $(PROGNAME);fi

