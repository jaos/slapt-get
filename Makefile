PROGRAM_NAME=slapt-get
VERSION=0.9.7b
ARCH=i386
RELEASE=1
CC=gcc
CURLFLAGS=`curl-config --libs`
OBJS=src/configuration.o src/package.o src/curl.o src/transaction.o src/action.o src/main.o
RCDEST=/etc/slapt-getrc
RCSOURCE=example.slapt-getrc
PROGRAMDIR=/usr/share/$(PROGRAM_NAME)
LOCALESDIR=$(PROGRAMDIR)/locales
SBINDIR=/sbin/
DEFINES=-DPROGRAM_NAME="\"$(PROGRAM_NAME)\"" -DVERSION="\"$(VERSION)\"" -DRC_LOCATION="\"$(RCDEST)\"" -DENABLE_NLS -DLOCALESDIR="\"$(LOCALESDIR)\""
CFLAGS=-W -Werror -Wall -O2 -ansi -pedantic -Iinclude $(DEFINES)

default: $(PROGRAM_NAME)

all: pkg

$(OBJS): 

$(PROGRAM_NAME): $(OBJS)
	$(CC) -o $(PROGRAM_NAME) $(OBJS) $(CFLAGS) $(CURLFLAGS)

static: $(OBJS)
	$(CC) -o $(PROGRAM_NAME) $(OBJS) $(CFLAGS) $(CURLFLAGS) -static

install: $(PROGRAM_NAME)
	install $(PROGRAM_NAME) $(SBINDIR)
	if [ ! -f $(RCDEST) ]; then install --mode=0644 -b $(RCSOURCE) $(RCDEST); else install --mode=0644 -b $(RCSOURCE) $(RCDEST).new;fi
	install $(PROGRAM_NAME).8 /usr/man/man8/
	gzip -f /usr/man/man8/$(PROGRAM_NAME).8
	install -d /var/$(PROGRAM_NAME)
	if [ ! -d $(LOCALESDIR)/en ]; then mkdir -p $(LOCALESDIR)/en/LC_MESSAGES; fi; msgfmt -o $(LOCALESDIR)/en/LC_MESSAGES/slapt-get.mo po/en.po;
	if [ ! -d $(LOCALESDIR)/pl ]; then mkdir -p $(LOCALESDIR)/pl/LC_MESSAGES; fi; msgfmt -o $(LOCALESDIR)/pl/LC_MESSAGES/slapt-get.mo po/pl.po;
	cp po/slapt-get.pot $(LOCALESDIR)/
	if [ ! -d /usr/doc/$(PROGRAM_NAME)-$(VERSION) ]; then mkdir /usr/doc/$(PROGRAM_NAME)-$(VERSION); fi
	cp example.slapt-getrc COPYING Changelog INSTALL README FAQ TODO /usr/doc/$(PROGRAM_NAME)-$(VERSION)/

uninstall:
	-rm /sbin/$(PROGRAM_NAME)
	-@echo leaving $(RCDEST)
	-rm /usr/man/man8/$(PROGRAM_NAME).8.gz
	-@echo leaving /var/$(PROGRAM_NAME)
	-rm -r /usr/doc/$(PROGRAM_NAME)-$(VERSION)
	-rm -r $(PROGRAMDIR)

clean:
	-if [ -f $(PROGRAM_NAME) ]; then rm $(PROGRAM_NAME);fi
	-rm src/*.o
	-if [ -d pkg ]; then rm -rf pkg ;fi

# add this back in later
#-@mkdir -p pkg/usr/lib
#-@mkdir -p pkg/usr/include/slapt
#-@cp include/*.h pkg/usr/include/slapt/
#-@mv pkg/usr/include/slapt/main.h pkg/usr/include/slapt/slapt.h
#-@cp src/libslapt-$(VERSION).a src/libslapt-$(VERSION).so pkg/usr/lib/
#-@strip pkg/usr/lib/libslapt-$(VERSION).so
#-@ln -s pkg/usr/lib/libslapt-$(VERSION).so pkg/usr/lib/libslapt.so

pkg: $(PROGRAM_NAME) libs
	-@mkdir pkg
	-@mkdir -p pkg/sbin
	-@mkdir -p pkg/etc
	-@mkdir -p pkg/install
	-@mkdir -p pkg/usr/man/man8
	-@mkdir -p pkg$(LOCALESDIR)
	-@mkdir -p pkg$(LOCALESDIR)/en/LC_MESSAGES; msgfmt -o pkg$(LOCALESDIR)/en/LC_MESSAGES/slapt-get.mo po/en.po
	-@mkdir -p pkg$(LOCALESDIR)/pl/LC_MESSAGES; msgfmt -o pkg$(LOCALESDIR)/pl/LC_MESSAGES/slapt-get.mo po/pl.po
	-@cp $(PROGRAM_NAME) ./pkg/sbin/
	-@cp po/slapt-get.pot pkg$(LOCALESDIR)/
	-@strip ./pkg/sbin/$(PROGRAM_NAME)
	-@cp example.slapt-getrc ./pkg/etc/slapt-getrc.new
	-@mkdir -p ./pkg/usr/doc/$(PROGRAM_NAME)-$(VERSION)/
	-@cp example.slapt-getrc COPYING Changelog INSTALL README FAQ TODO ./pkg/usr/doc/$(PROGRAM_NAME)-$(VERSION)/
	-@echo "if [ ! -f etc/slapt-getrc ]; then mv etc/slapt-getrc.new etc/slapt-getrc; else diff -q etc/slapt-getrc etc/slapt-getrc.new >/dev/null 2>&1 && rm etc/slapt-getrc.new; fi" > pkg/install/doinst.sh
	-@cp slack-desc pkg/install/
	-@cp slack-required pkg/install/
	-@cp $(PROGRAM_NAME).8 pkg/usr/man/man8/
	-@gzip pkg/usr/man/man8/$(PROGRAM_NAME).8
	@( cd pkg; makepkg -l y -c y $(PROGRAM_NAME)-$(VERSION)-$(ARCH)-$(RELEASE).tgz )

po_file:
	-grep '_(' src/*.c |cut -f2-255 -d':'|sed -re "s/.*(_\(\".*\"\)).*/\1/" > po/gettext_strings
	-mv po/slapt-get.pot po/slapt-get.pot~
	-xgettext -d slapt-get -o po/slapt-get.pot -a -C --no-location po/gettext_strings
	-rm po/gettext_strings

libs:
	$(CC) -shared -o src/libslapt-$(VERSION).so $(OBJS)
	ar -r src/libslapt-$(VERSION).a $(OBJS)

