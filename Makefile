PROGRAM_NAME=slapt-get
VERSION=0.9.4
CC=gcc
CURLFLAGS=`curl-config --libs`
OBJS=src/configuration.o src/package.o src/curl.o src/action.o src/main.o
RCDEST=/etc/slapt-getrc
RCSOURCE=example.slapt-getrc
SBINDIR=/sbin/
DEFINES=-DPROGRAM_NAME="\"$(PROGRAM_NAME)\"" -DVERSION="\"$(VERSION)\"" -DRC_LOCATION="\"$(RCDEST)\""
CFLAGS=-W -Werror -Wall -O2 -ansi -pedantic -Iinclude $(DEFINES)

default: $(PROGRAM_NAME)

all: pkg

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
	-if [ -d pkg ]; then rm -rf pkg ;fi

pkg: $(PROGRAM_NAME)
	-@mkdir pkg
	-@mkdir -p pkg/sbin
	-@mkdir -p pkg/etc
	-@mkdir -p pkg/install
	-@mkdir -p pkg/usr/man/man8
	-@cp $(PROGRAM_NAME) ./pkg/sbin/
	-@cp example.slapt-getrc ./pkg/etc/slapt-getrc
	-@mkdir -p ./pkg/usr/doc/$(PROGRAM_NAME)-$(VERSION)/
	-@cp COPYING Changelog INSTALL README FAQ TODO ./pkg/usr/doc/$(PROGRAM_NAME)-$(VERSION)/
	-@cp slack-desc pkg/install/
	-@cp $(PROGRAM_NAME).8 pkg/usr/man/man8/
	@( cd pkg; makepkg -c y $(PROGRAM_NAME)-$(VERSION).tgz )

