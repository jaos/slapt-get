PROGNAME=jaospkg
VERSION=0.8
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

install: $(PROGNAME)
	install $(PROGNAME) $(SBINDIR)
	install --mode=0644 -b $(RCSOURCE) $(RCDEST)
	install $(PROGNAME).8 /usr/man/man8/

uninstall:
	-rm /sbin/$(PROGNAME)
	-@echo leaving $(RCDEST)

clean:
	-if [ -f $(PROGNAME) ]; then rm $(PROGNAME);fi
	-rm src/*.o
	-if [ -d slackpkg ]; then rm -rf slackpkg ;fi

slackpkg: $(PROGNAME)
	-@mkdir slackpkg
	-@mkdir -p slackpkg/sbin
	-@mkdir -p slackpkg/etc
	-@mkdir -p slackpkg/install
	-@mkdir -p slackpkg/usr/man/man8
	-@cp $(PROGNAME) ./slackpkg/sbin/
	-@cp example.jaospkgrc ./slackpkg/etc/jaospkgrc
	-@mkdir -p ./slackpkg/usr/doc/$(PROGNAME)-$(VERSION)/
	-@cp COPYING Changelog INSTALL README TODO ./slackpkg/usr/doc/$(PROGNAME)-$(VERSION)/
	-@cp slack-desc slackpkg/install/
	-@cp $(PROGNAME).8 slackpkg/usr/man/man8/
	@( cd slackpkg; makepkg -c y $(PROGNAME)-$(VERSION).tgz )

