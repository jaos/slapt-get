PROGRAM_NAME=slapt-get
VERSION=0.9.8j
ARCH=i386
RELEASE=1
CC=gcc
CURLFLAGS=`curl-config --libs`
OBJS=src/common.o src/configuration.o src/package.o src/curl.o src/transaction.o src/action.o src/main.o
LIBOBJS=src/common.o src/configuration.o src/package.o src/curl.o src/transaction.o
NONLIBOBJS=src/action.o src/main.o
RCDEST=/etc/slapt-getrc
RCSOURCE=example.slapt-getrc
LOCALESDIR=/usr/share/locale
SBINDIR=/sbin/
DEFINES=-DPROGRAM_NAME="\"$(PROGRAM_NAME)\"" -DVERSION="\"$(VERSION)\"" -DRC_LOCATION="\"$(RCDEST)\"" -DENABLE_NLS -DLOCALESDIR="\"$(LOCALESDIR)\""
CFLAGS=-W -Werror -Wall -O2 -ansi -pedantic -Iinclude $(DEFINES)

default: $(PROGRAM_NAME)

all: pkg

$(OBJS): 

$(PROGRAM_NAME): libs
	$(CC) -o $(PROGRAM_NAME) $(OBJS) $(CFLAGS) $(CURLFLAGS)

#test with LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./src ./slapt-get
withlibslapt: libs
	$(CC) -o $(PROGRAM_NAME) $(NONLIBOBJS) $(CFLAGS) $(CURLFLAGS) -L./src -lslapt-$(VERSION)

static: libs
	$(CC) -o $(PROGRAM_NAME) $(OBJS) $(CFLAGS) $(CURLFLAGS) -static

install: $(PROGRAM_NAME) doinstall

staticinstall: static doinstall

withlibslaptinstall: withlibslapt doinstall

doinstall:
	install $(PROGRAM_NAME) $(SBINDIR)
	chown root:bin $(SBINDIR)$(PROGRAM_NAME)
	if [ ! -f $(RCDEST) ]; then install --mode=0644 -b $(RCSOURCE) $(RCDEST); else install --mode=0644 -b $(RCSOURCE) $(RCDEST).new;fi
	install $(PROGRAM_NAME).8 /usr/man/man8/
	gzip -f /usr/man/man8/$(PROGRAM_NAME).8
	install -d /var/$(PROGRAM_NAME)
	for i in `ls po/ --ignore=slapt-get.pot --ignore=CVS |sed 's/.po//'` ;do if [ ! -d $(LOCALESDIR)/$$i/LC_MESSAGES ]; then mkdir -p $(LOCALESDIR)/$$i/LC_MESSAGES; fi; msgfmt -o $(LOCALESDIR)/$$i/LC_MESSAGES/slapt-get.mo po/$$i.po;done
	mkdir -p /usr/doc/$(PROGRAM_NAME)-$(VERSION)/
	cp example.slapt-getrc COPYING Changelog INSTALL README FAQ TODO /usr/doc/$(PROGRAM_NAME)-$(VERSION)/
	cp include/slapt.h /usr/include/
	cp src/libslapt-$(VERSION).a src/libslapt-$(VERSION).so /usr/lib/
	if [ -L /usr/lib/libslapt.so ]; then rm /usr/lib/libslapt.so;fi
	ln -s /usr/lib/libslapt-$(VERSION).so /usr/lib/libslapt.so

uninstall:
	-rm /sbin/$(PROGRAM_NAME)
	-@echo leaving $(RCDEST)
	-rm /usr/man/man8/$(PROGRAM_NAME).8.gz
	-@echo leaving /var/$(PROGRAM_NAME)
	-rm -r /usr/doc/$(PROGRAM_NAME)-$(VERSION)
	-find /usr/share/locale/ -name 'slapt-get.mo' -exec rm {} \;
	-rm /usr/include/slapt.h
	-readlink /usr/lib/libslapt.so|xargs -r rm
	-rm /usr/lib/libslapt.so

clean:
	-if [ -f $(PROGRAM_NAME) ]; then rm $(PROGRAM_NAME);fi
	-rm src/*.o
	-rm src/*.a
	-rm src/*.so
	-rm include/slapt.h
	-if [ -d pkg ]; then rm -rf pkg ;fi
	-if [ -f libs ]; then rm -rf libs ;fi


pkg: $(PROGRAM_NAME) dopkg

staticpkg: static dopkg

withlibslaptpkg: withlibslapt dopkg

dopkg:
	@mkdir pkg
	@mkdir -p pkg/sbin
	@mkdir -p pkg/etc
	@mkdir -p pkg/install
	@mkdir -p pkg/usr/man/man8
	@for i in `ls po/ --ignore=slapt-get.pot --ignore=CVS |sed 's/.po//'` ;do mkdir -p pkg$(LOCALESDIR)/$$i/LC_MESSAGES; msgfmt -o pkg$(LOCALESDIR)/$$i/LC_MESSAGES/slapt-get.mo po/$$i.po; done
	@cp $(PROGRAM_NAME) ./pkg/sbin/
	@chown root:bin ./pkg/sbin/$(PROGRAM_NAME)
	@strip ./pkg/sbin/$(PROGRAM_NAME)
	@echo "# See /usr/doc/$(PROGRAM_NAME)-$(VERSION)/example.slapt-getrc " > ./pkg/etc/slapt-getrc.new
	@echo "# for example source entries and configuration hints." >> ./pkg/etc/slapt-getrc.new
	@cat example.slapt-getrc |grep -v '^#'|grep -v '^$$' >> ./pkg/etc/slapt-getrc.new
	@mkdir -p ./pkg/usr/doc/$(PROGRAM_NAME)-$(VERSION)/
	@cp example.slapt-getrc COPYING Changelog INSTALL README FAQ TODO ./pkg/usr/doc/$(PROGRAM_NAME)-$(VERSION)/
	@echo "if [ ! -f etc/slapt-getrc ]; then mv etc/slapt-getrc.new etc/slapt-getrc; else diff -q etc/slapt-getrc etc/slapt-getrc.new >/dev/null 2>&1 && rm etc/slapt-getrc.new; fi; if [ -L usr/lib/libslapt.so ]; then rm usr/lib/libslapt.so;fi; ln -s usr/lib/libslapt-$(VERSION).so usr/lib/libslapt.so" > pkg/install/doinst.sh
	@cp slack-desc pkg/install/
	@cp slack-required pkg/install/
	@cp $(PROGRAM_NAME).8 pkg/usr/man/man8/
	@gzip pkg/usr/man/man8/$(PROGRAM_NAME).8
	@mkdir -p pkg/usr/lib
	@mkdir -p pkg/usr/include
	@cp include/slapt.h pkg/usr/include/
	@cp src/libslapt-$(VERSION).a src/libslapt-$(VERSION).so pkg/usr/lib/
	@strip pkg/usr/lib/libslapt-$(VERSION).so
	@( cd pkg; /sbin/makepkg -l y -c y $(PROGRAM_NAME)-$(VERSION)-$(ARCH)-$(RELEASE).tgz )

po_file:
	-grep '_(' src/*.c |cut -f2-255 -d':'|sed -re "s/.*(_\(\".*\"\)).*/\1/" > po/gettext_strings
	-mv po/slapt-get.pot po/slapt-get.pot~
	-xgettext -d slapt-get -o po/slapt-get.pot -a -C --no-location po/gettext_strings
	-rm po/gettext_strings

libs: $(OBJS)
	touch libs
	$(CC) -shared -o src/libslapt-$(VERSION).so $(LIBOBJS)
	ar -r src/libslapt-$(VERSION).a src/configuration.o src/package.o src/curl.o src/transaction.o
	cat include/main.h include/configuration.h include/package.h include/curl.h include/transaction.h |grep -v '#include \"' > include/slapt.h

