PROGRAM_NAME=slapt-get
VERSION=0.9.2
CC=gcc
CURLFLAGS=`curl-config --libs`
OBJS=src/configuration.o src/package.o src/curl.o src/action.o src/main.o
RCDEST=/etc/slapt-getrc
RCSOURCE=example.slapt-getrc
SBINDIR=/sbin/
DEFINES=-DPROGRAM_NAME="\"$(PROGRAM_NAME)\"" -DVERSION="\"$(VERSION)\"" -DRC_LOCATION="\"$(RCDEST)\""
CFLAGS=-W -Werror -Wall -O2 -ansi -pedantic -Iinclude $(DEFINES)

default: all

all: slackpkg

$(OBJS): 

$(PROGRAM_NAME): $(OBJS)
	$(CC) $(CFLAGS) $(CURLFLAGS) -o $(PROGRAM_NAME) $(OBJS)

install: $(PROGRAM_NAME)
	install $(PROGRAM_NAME) $(SBINDIR)
	install --mode=0644 -b $(RCSOURCE) $(RCDEST)
	install $(PROGRAM_NAME).8 /usr/man/man8/

uninstall:
	-rm /sbin/$(PROGRAM_NAME)
	-@echo leaving $(RCDEST)

clean:
	-if [ -f $(PROGRAM_NAME) ]; then rm $(PROGRAM_NAME);fi
	-rm src/*.o
	-if [ -d slackpkg ]; then rm -rf slackpkg ;fi

slackpkg: $(PROGRAM_NAME)
	-@mkdir slackpkg
	-@mkdir -p slackpkg/sbin
	-@mkdir -p slackpkg/etc
	-@mkdir -p slackpkg/install
	-@mkdir -p slackpkg/usr/man/man8
	-@cp $(PROGRAM_NAME) ./slackpkg/sbin/
	-@cp example.slapt-getrc ./slackpkg/etc/slapt-getrc
	-@mkdir -p ./slackpkg/usr/doc/$(PROGRAM_NAME)-$(VERSION)/
	-@cp COPYING Changelog INSTALL README TODO ./slackpkg/usr/doc/$(PROGRAM_NAME)-$(VERSION)/
	-@cp slack-desc slackpkg/install/
	-@cp $(PROGRAM_NAME).8 slackpkg/usr/man/man8/
	@( cd slackpkg; makepkg -c y $(PROGRAM_NAME)-$(VERSION).tgz )

