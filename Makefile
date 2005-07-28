PACKAGE=slapt-get
VERSION=0.9.10b
ARCH=$(shell uname -m | sed -e "s/i[3456]86/i386/")
LIBDIR=/usr/lib
RELEASE=1
CC=gcc
CURLFLAGS=`curl-config --libs`
OBJS=src/common.o src/configuration.o src/package.o src/curl.o src/transaction.o src/action.o src/main.o
LIBOBJS=src/common.o src/configuration.o src/package.o src/curl.o src/transaction.o
NONLIBOBJS=src/action.o src/main.o
RCDEST=/etc/slapt-get/slapt-getrc
RCSOURCE=example.slapt-getrc
PACKAGE_LOCALE_DIR=/usr/share/locale
SBINDIR=/usr/sbin/
GETTEXT_PACKAGE=$(PACKAGE)
DEFINES=-DPACKAGE="\"$(PACKAGE)\"" -DVERSION="\"$(VERSION)\"" -DRC_LOCATION="\"$(RCDEST)\"" -DENABLE_NLS -DPACKAGE_LOCALE_DIR="\"$(PACKAGE_LOCALE_DIR)\"" -DGETTEXT_PACKAGE="\"$(GETTEXT_PACKAGE)\""
CFLAGS=-W -Werror -Wall -O2 -ansi -pedantic $(DEFINES) -fpic
LDFLAGS=$(CURLFLAGS) -lz
ifeq ($(ARCH),x86_64)
	LIBDIR=/usr/lib64
	CFLAGS=-W -Werror -Wall -O2 -ansi -pedantic $(DEFINES) -fPIC
endif

default: $(PACKAGE)

all: pkg

$(OBJS): 

$(PACKAGE): libs
	$(CC) -o $(PACKAGE) $(OBJS) $(CFLAGS) $(LDFLAGS)

withlibslapt: libs
	$(CC) -o $(PACKAGE) $(NONLIBOBJS) $(CFLAGS) $(LDFLAGS) -lslapt-$(VERSION)

static: libs
	$(CC) -o $(PACKAGE) $(OBJS) $(CFLAGS) $(LDFLAGS) -static

install: $(PACKAGE) doinstall

staticinstall: static doinstall

withlibslaptinstall: withlibslapt doinstall

libsinstall: libs
	if [ ! -d $(DESTDIR)/usr/include ]; then mkdir -p $(DESTDIR)/usr/include;fi
	cp src/slapt.h $(DESTDIR)/usr/include/
	if [ ! -d $(DESTDIR)$(LIBDIR) ]; then mkdir -p $(DESTDIR)$(LIBDIR);fi
	cp src/libslapt-$(VERSION).a src/libslapt-$(VERSION).so $(DESTDIR)$(LIBDIR)/
	if [ -L $(DESTDIR)$(LIBDIR)/libslapt.so ]; then rm $(DESTDIR)$(LIBDIR)/libslapt.so;fi
	cd $(DESTDIR)$(LIBDIR); ln -s libslapt-$(VERSION).so libslapt.so
	if [ -L $(DESTDIR)$(LIBDIR)/libslapt.a ]; then rm $(DESTDIR)$(LIBDIR)/libslapt.a;fi
	cd $(DESTDIR)$(LIBDIR); ln -s libslapt-$(VERSION).a libslapt.a

doinstall: libsinstall
	strip --strip-unneeded $(PACKAGE)
	if [ ! -d $(DESTDIR)$(SBINDIR) ]; then mkdir -p $(DESTDIR)$(SBINDIR);fi
	install $(PACKAGE) $(DESTDIR)$(SBINDIR)
	-chown root:bin $(DESTDIR)$(SBINDIR)$(PACKAGE)
	if [ ! -d $(DESTDIR)/etc/slapt-get ]; then mkdir -p $(DESTDIR)/etc/slapt-get;fi
	if [ -f $(DESTDIR)/etc/slapt-getrc ]; then mv $(DESTDIR)/etc/slapt-getrc $(DESTDIR)$(RCDEST);fi
	if [ ! -f $(DESTDIR)$(RCDEST) ]; then install --mode=0644 -b $(RCSOURCE) $(DESTDIR)$(RCDEST); else install --mode=0644 -b $(RCSOURCE) $(DESTDIR)$(RCDEST).new;fi
	if [ ! -d $(DESTDIR)/usr/man ]; then mkdir -p $(DESTDIR)/usr/man;fi
	if [ ! -d $(DESTDIR)/usr/man/man8 ]; then mkdir -p $(DESTDIR)/usr/man/man8;fi
	install $(PACKAGE).8 $(DESTDIR)/usr/man/man8/
	gzip -f $(DESTDIR)/usr/man/man8/$(PACKAGE).8
	install -d $(DESTDIR)/var/$(PACKAGE)
	for i in `ls po/ --ignore=slapt-get.pot --ignore=CVS |sed 's/.po//'` ;do if [ ! -d $(DESTDIR)$(PACKAGE_LOCALE_DIR)/$$i/LC_MESSAGES ]; then mkdir -p $(DESTDIR)$(PACKAGE_LOCALE_DIR)/$$i/LC_MESSAGES; fi; msgfmt -o $(DESTDIR)$(PACKAGE_LOCALE_DIR)/$$i/LC_MESSAGES/slapt-get.mo po/$$i.po;done
	mkdir -p $(DESTDIR)/usr/doc/$(PACKAGE)-$(VERSION)/
	cp example.slapt-getrc COPYING ChangeLog INSTALL README FAQ FAQ.html TODO $(DESTDIR)/usr/doc/$(PACKAGE)-$(VERSION)/

uninstall:
	-rm /$(SBINDIR)/$(PACKAGE)
	-@echo leaving $(RCDEST)
	-rm /usr/man/man8/$(PACKAGE).8.gz
	-@echo leaving /var/$(PACKAGE)
	-rm -r /usr/doc/$(PACKAGE)-$(VERSION)
	-find /usr/share/locale/ -name 'slapt-get.mo' -exec rm {} \;
	-rm /usr/include/slapt.h
	-readlink $(LIBDIR)/libslapt.so|xargs -r rm
	-rm $(LIBDIR)/libslapt.so

clean:
	-if [ -f $(PACKAGE) ]; then rm $(PACKAGE);fi
	-rm src/*.o
	-rm src/*.a
	-rm src/*.so
	-rm src/slapt.h
	-if [ -d pkg ]; then rm -rf pkg ;fi
	-if [ -f libs ]; then rm -rf libs ;fi


pkg: $(PACKAGE) dopkg

staticpkg: static dopkg

withlibslaptpkg: withlibslapt dopkg

dopkg:
	mkdir pkg
	mkdir -p pkg/$(SBINDIR)
	mkdir -p pkg/etc/slapt-get
	mkdir -p pkg/install
	mkdir -p pkg/usr/man/man8
	for i in `ls po/ --ignore=slapt-get.pot --ignore=CVS |sed 's/.po//'` ;do mkdir -p pkg$(PACKAGE_LOCALE_DIR)/$$i/LC_MESSAGES; msgfmt -o pkg$(PACKAGE_LOCALE_DIR)/$$i/LC_MESSAGES/slapt-get.mo po/$$i.po; done
	cp $(PACKAGE) ./pkg/$(SBINDIR)
	-chown root:bin ./pkg/$(SBINDIR)
	-chown root:bin ./pkg/$(SBINDIR)/$(PACKAGE)
	strip ./pkg/$(SBINDIR)/$(PACKAGE)
	echo "# See /usr/doc/$(PACKAGE)-$(VERSION)/example.slapt-getrc " > ./pkg/etc/slapt-get/slapt-getrc.new
	echo "# for example source entries and configuration hints." >> ./pkg/etc/slapt-get/slapt-getrc.new
	cat example.slapt-getrc |grep -v '^#'|grep -v '^$$' >> ./pkg/etc/slapt-get/slapt-getrc.new
	mkdir -p ./pkg/usr/doc/$(PACKAGE)-$(VERSION)/
	cp example.slapt-getrc COPYING ChangeLog INSTALL README FAQ FAQ.html TODO ./pkg/usr/doc/$(PACKAGE)-$(VERSION)/
	echo "if [ ! -d etc/slapt-get ]; then mkdir -p etc/slapt-get; fi; if [ -f etc/slapt-getrc -a ! -f etc/slapt-get/slapt-getrc ]; then mv etc/slapt-getrc etc/slapt-get/slapt-getrc; fi; if [ ! -f etc/slapt-get/slapt-getrc ]; then mv etc/slapt-get/slapt-getrc.new etc/slapt-get/slapt-getrc; else sed -re 's/(See \/usr\/doc\/slapt\-get\-).*(\/example\.slapt\-getrc)/\1$(VERSION)\2/' /etc/slapt-get/slapt-getrc > /tmp/tmp_slapt-getrc_tmp; cat /tmp/tmp_slapt-getrc_tmp > /etc/slapt-get/slapt-getrc; rm /tmp/tmp_slapt-getrc_tmp; diff -q etc/slapt-get/slapt-getrc etc/slapt-get/slapt-getrc.new >/dev/null 2>&1 && rm etc/slapt-get/slapt-getrc.new; fi;" > pkg/install/doinst.sh
	cp slack-desc pkg/install/
	cp slack-required pkg/install/
	cp slack-suggests pkg/install/
	cp $(PACKAGE).8 pkg/usr/man/man8/
	gzip pkg/usr/man/man8/$(PACKAGE).8
	mkdir -p pkg$(LIBDIR)
	mkdir -p pkg/usr/include
	cp src/slapt.h pkg/usr/include/
	cp src/libslapt-$(VERSION).a src/libslapt-$(VERSION).so pkg$(LIBDIR)/
	strip pkg$(LIBDIR)/libslapt-$(VERSION).so
	( cd pkg$(LIBDIR); ln -s libslapt-$(VERSION).so libslapt.so; ln -s libslapt-$(VERSION).a libslapt.a )
	-( cd pkg; /sbin/makepkg -l y -c n $(PACKAGE)-$(VERSION)-$(ARCH)-$(RELEASE).tgz )

po_file:
	-grep '_(' src/*.c |cut -f2-255 -d':'|sed -re "s/.*(_\(\".*\"\)).*/\1/" > po/gettext_strings
	-mv po/slapt-get.pot po/slapt-get.pot~
	-xgettext -d slapt-get -o po/slapt-get.pot -a -C --no-location po/gettext_strings
	-rm po/gettext_strings

libs: $(OBJS)
	touch libs
	$(CC) -shared -o src/libslapt-$(VERSION).so $(LIBOBJS)
	ar -r src/libslapt-$(VERSION).a $(LIBOBJS)
	cat src/main.h src/common.h src/configuration.h src/package.h src/curl.h src/transaction.h |grep -v '#include \"' > src/slapt.h

