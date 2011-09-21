PACKAGE=slapt-get
VERSION=0.10.2l
ARCH?=$(shell uname -m | sed -e "s/i[3456]86/i386/")
LIBDIR=/usr/lib
RELEASE=1
CC?=gcc
STRIP?=strip
AR?=ar
RANLIB?=ranlib
CURLFLAGS=`curl-config --libs`
OBJS=src/common.o src/configuration.o src/package.o src/curl.o src/transaction.o src/action.o src/main.o
LIBOBJS=src/common.o src/configuration.o src/package.o src/curl.o src/transaction.o
LIBHEADERS=src/main.h src/common.h src/configuration.h src/package.h src/curl.h src/transaction.h
NONLIBOBJS=src/action.o src/main.o
RCDEST=/etc/slapt-get/slapt-getrc
RCSOURCE=default.slapt-getrc.$(ARCH)
PACKAGE_LOCALE_DIR=/usr/share/locale
SBINDIR=/usr/sbin/
GETTEXT_PACKAGE=$(PACKAGE)
DEFINES=-DPACKAGE="\"$(PACKAGE)\"" -DVERSION="\"$(VERSION)\"" -DRC_LOCATION="\"$(RCDEST)\"" -DENABLE_NLS -DPACKAGE_LOCALE_DIR="\"$(PACKAGE_LOCALE_DIR)\"" -DGETTEXT_PACKAGE="\"$(GETTEXT_PACKAGE)\""
LDFLAGS+=$(CURLFLAGS) -lz
HAS_GPGME=$(shell gpgme-config --libs 2>&1 >/dev/null && echo 1)
ifeq ($(HAS_GPGME),1)
	DEFINES+=-DSLAPT_HAS_GPGME
	OBJS+=src/gpgme.o
	LIBOBJS+=src/gpgme.o
	LIBHEADERS+=src/gpgme.h
	LDFLAGS+=`gpgme-config --libs`
endif
CFLAGS?=-W -Werror -Wall -O2 -ansi -pedantic
CFLAGS+=$(DEFINES) -fPIC
ifeq ($(ARCH),x86_64)
	LIBDIR=/usr/lib64
endif

default: $(PACKAGE)

all: pkg

$(OBJS): $(LIBHEADERS)

$(PACKAGE): libs
	$(CC) -o $(PACKAGE) $(OBJS) $(CFLAGS) $(LDFLAGS)

withlibslapt: libs
	$(CC) -o $(PACKAGE) $(NONLIBOBJS) -L./src -Wl,-R$(LIBDIR) $(CFLAGS) $(LDFLAGS) -lslapt

static: libs
	$(CC) -o $(PACKAGE) $(OBJS) $(CFLAGS) $(LDFLAGS) -static

install: $(PACKAGE) doinstall

staticinstall: static doinstall

withlibslaptinstall: withlibslapt doinstall

libsinstall: libs
	if [ ! -d $(DESTDIR)/usr/include ]; then mkdir -p $(DESTDIR)/usr/include;fi
	cp -f src/slapt.h $(DESTDIR)/usr/include/
	if [ ! -d $(DESTDIR)$(LIBDIR) ]; then mkdir -p $(DESTDIR)$(LIBDIR);fi
	if [ -L $(DESTDIR)$(LIBDIR)/libslapt.so ]; then rm $(DESTDIR)$(LIBDIR)/libslapt.so;fi
	if [ -L $(DESTDIR)$(LIBDIR)/libslapt.a ]; then rm $(DESTDIR)$(LIBDIR)/libslapt.a;fi
	cp -f src/libslapt.a src/libslapt.so.$(VERSION) $(DESTDIR)$(LIBDIR)/
	cd $(DESTDIR)$(LIBDIR); ln -sf libslapt.so.$(VERSION) libslapt.so

doinstall: libsinstall
	$(STRIP) --strip-unneeded $(PACKAGE)
	if [ ! -d $(DESTDIR)$(SBINDIR) ]; then mkdir -p $(DESTDIR)$(SBINDIR);fi
	install $(PACKAGE) $(DESTDIR)$(SBINDIR)
	-chown $$(stat --format "%u:%g" /usr/sbin) $(DESTDIR)$(SBINDIR)$(PACKAGE)
	if [ ! -d $(DESTDIR)/etc/slapt-get ]; then mkdir -p $(DESTDIR)/etc/slapt-get;fi
	if [ -f $(DESTDIR)/etc/slapt-getrc ]; then mv -f $(DESTDIR)/etc/slapt-getrc $(DESTDIR)$(RCDEST);fi
	if [ ! -f $(DESTDIR)$(RCDEST) ]; then install --mode=0644 -b $(RCSOURCE) $(DESTDIR)$(RCDEST); else install --mode=0644 -b $(RCSOURCE) $(DESTDIR)$(RCDEST).new;fi
	if [ ! -d $(DESTDIR)/usr/man ]; then mkdir -p $(DESTDIR)/usr/man;fi
	if [ ! -d $(DESTDIR)/usr/man/ru ]; then mkdir -p $(DESTDIR)/usr/man/ru;fi
	if [ ! -d $(DESTDIR)/usr/man/uk ]; then mkdir -p $(DESTDIR)/usr/man/uk;fi
	if [ ! -d $(DESTDIR)/usr/man/man8 ]; then mkdir -p $(DESTDIR)/usr/man/man8;fi
	if [ ! -d $(DESTDIR)/usr/man/man3 ]; then mkdir -p $(DESTDIR)/usr/man/man3;fi
	if [ ! -d $(DESTDIR)/usr/man/ru/man8 ]; then mkdir -p $(DESTDIR)/usr/man/ru/man8;fi
	if [ ! -d $(DESTDIR)/usr/man/uk/man8 ]; then mkdir -p $(DESTDIR)/usr/man/uk/man8;fi
	install --mode=0644 doc/$(PACKAGE).8 $(DESTDIR)/usr/man/man8/
	install --mode=0644 doc/libslapt.3 $(DESTDIR)/usr/man/man3/
	install --mode=0644 doc/$(PACKAGE).ru.8 $(DESTDIR)/usr/man/ru/man8/$(PACKAGE).8
	install --mode=0644 doc/$(PACKAGE).uk.8 $(DESTDIR)/usr/man/uk/man8/$(PACKAGE).8
	gzip -f $(DESTDIR)/usr/man/man8/$(PACKAGE).8
	gzip -f $(DESTDIR)/usr/man/man3/libslapt.3
	gzip -f $(DESTDIR)/usr/man/ru/man8/$(PACKAGE).8
	gzip -f $(DESTDIR)/usr/man/uk/man8/$(PACKAGE).8
	install -d $(DESTDIR)/var/$(PACKAGE)
	for i in `ls po/*.po | sed 's/.po//' | xargs -n1 basename` ;do if [ ! -d $(DESTDIR)$(PACKAGE_LOCALE_DIR)/$$i/LC_MESSAGES ]; then mkdir -p $(DESTDIR)$(PACKAGE_LOCALE_DIR)/$$i/LC_MESSAGES; fi; msgfmt -o $(DESTDIR)$(PACKAGE_LOCALE_DIR)/$$i/LC_MESSAGES/slapt-get.mo po/$$i.po;done
	mkdir -p $(DESTDIR)/usr/doc/$(PACKAGE)-$(VERSION)/
	cp -f default.slapt-getrc.* example.slapt-getrc.* COPYING ChangeLog INSTALL README FAQ FAQ.html TODO $(DESTDIR)/usr/doc/$(PACKAGE)-$(VERSION)/

uninstall:
	-rm /$(SBINDIR)/$(PACKAGE)
	-@echo leaving $(RCDEST)
	-rm /usr/man/man8/$(PACKAGE).8.gz
	-rm /usr/man/man3/libslapt.3.gz
	-rm /usr/man/ru/man8/$(PACKAGE).8.gz
	-rm /usr/man/uk/man8/$(PACKAGE).8.gz
	-@echo leaving /var/$(PACKAGE)
	-rm -r /usr/doc/$(PACKAGE)-$(VERSION)
	-find /usr/share/locale/ -name 'slapt-get.mo' -exec rm {} \;
	-rm /usr/include/slapt.h
	-readlink $(LIBDIR)/libslapt.so|xargs -r rm
	-rm $(LIBDIR)/libslapt.so

clean: testclean
	-if [ -f $(PACKAGE) ]; then rm $(PACKAGE);fi
	-rm src/*.o
	-rm src/*.a
	-rm src/*.so
	-rm src/libslapt.so*
	-rm src/slapt.h
	-rm slapt-get-*.tgz
	-if [ -d pkg ]; then rm -rf pkg ;fi
	-if [ -f libs ]; then rm -rf libs ;fi


.PHONY: pkg dopkg
pkg: $(PACKAGE) dopkg

staticpkg: static dopkg

withlibslaptpkg: withlibslapt dopkg

dopkg:
	mkdir -p pkg
	mkdir -p pkg/$(SBINDIR)
	mkdir -p pkg/etc/slapt-get
	mkdir -p pkg/install
	mkdir -p pkg/usr/man/man8
	mkdir -p pkg/usr/man/man3
	mkdir -p pkg/usr/man/ru/man8
	mkdir -p pkg/usr/man/uk/man8
	for i in `ls po/ --ignore=slapt-get.pot --ignore=CVS |sed 's/.po//'` ;do mkdir -p pkg$(PACKAGE_LOCALE_DIR)/$$i/LC_MESSAGES; msgfmt -o pkg$(PACKAGE_LOCALE_DIR)/$$i/LC_MESSAGES/slapt-get.mo po/$$i.po; done
	cp -f $(PACKAGE) ./pkg/$(SBINDIR)
	-chown $$(stat --format "%u:%g" /usr/sbin) ./pkg/$(SBINDIR)
	-chown $$(stat --format "%u:%g" /usr/sbin) ./pkg/$(SBINDIR)/$(PACKAGE)
	$(STRIP) ./pkg/$(SBINDIR)/$(PACKAGE)
	cp -f $(RCSOURCE) pkg/etc/slapt-get/slapt-getrc.new
	mkdir -p ./pkg/usr/doc/$(PACKAGE)-$(VERSION)/
	cp -f default.slapt-getrc.* example.slapt-getrc.* COPYING ChangeLog INSTALL README FAQ FAQ.html TODO ./pkg/usr/doc/$(PACKAGE)-$(VERSION)/
	echo "if [ ! -d etc/slapt-get ]; then mkdir -p etc/slapt-get; fi; if [ -f etc/slapt-getrc -a ! -f etc/slapt-get/slapt-getrc ]; then mv -f etc/slapt-getrc etc/slapt-get/slapt-getrc; fi; if [ ! -f etc/slapt-get/slapt-getrc ]; then mv -f etc/slapt-get/slapt-getrc.new etc/slapt-get/slapt-getrc; else sed -re 's/(See \/usr\/doc\/slapt\-get\-).*(\/example\.slapt\-getrc)/\1$(VERSION)\2/' /etc/slapt-get/slapt-getrc > /tmp/tmp_slapt-getrc_tmp; cat /tmp/tmp_slapt-getrc_tmp > /etc/slapt-get/slapt-getrc; rm /tmp/tmp_slapt-getrc_tmp; diff -q etc/slapt-get/slapt-getrc etc/slapt-get/slapt-getrc.new >/dev/null 2>&1 && rm etc/slapt-get/slapt-getrc.new; fi;" > pkg/install/doinst.sh
	cp -f slack-desc pkg/install/
	cp -f slack-required pkg/install/
	cp -f slack-suggests pkg/install/
	cp -f doc/$(PACKAGE).8 pkg/usr/man/man8/
	cp -f doc/libslapt.3 pkg/usr/man/man3/
	cp -f doc/$(PACKAGE).ru.8 pkg/usr/man/ru/man8/$(PACKAGE).8
	cp -f doc/$(PACKAGE).uk.8 pkg/usr/man/uk/man8/$(PACKAGE).8
	gzip -f pkg/usr/man/man8/$(PACKAGE).8
	gzip -f pkg/usr/man/man3/libslapt.3
	gzip -f pkg/usr/man/ru/man8/$(PACKAGE).8
	gzip -f pkg/usr/man/uk/man8/$(PACKAGE).8
	mkdir -p pkg$(LIBDIR)
	mkdir -p pkg/usr/include
	cp -f src/slapt.h pkg/usr/include/
	cp -f src/libslapt.a src/libslapt.so.$(VERSION) pkg$(LIBDIR)/
	$(STRIP) pkg$(LIBDIR)/libslapt.so.$(VERSION)
	( cd pkg$(LIBDIR); ln -sf libslapt.so.$(VERSION) libslapt.so )
	-( cd pkg; /sbin/makepkg -l y -c n ../$(PACKAGE)-$(VERSION)-$(ARCH)-$(RELEASE).tgz )

po_file:
	-xgettext -o po/slapt-get.pot.new -sC --no-location src/*.c src/*.h
	-if [ ! -f po/slapt-get.pot ]; then mv po/slapt-get.pot.new po/slapt-get.pot; else msgmerge -Us po/slapt-get.pot po/slapt-get.pot.new ; fi
	-rm po/slapt-get.pot.new
	-for i in `ls po/*.po`; do msgmerge -UNs $$i po/slapt-get.pot; done

libs: $(OBJS)
	touch libs
	$(CC) -shared -o src/libslapt.so.$(VERSION) $(LIBOBJS) #-Wl,-soname=libslapt-$(VERSION)
	( cd src; if [ -f libslapt.so ]; then rm libslapt.so;fi; ln -sf libslapt.so.$(VERSION) libslapt.so )
	$(AR) -r src/libslapt.a $(LIBOBJS)
	$(RANLIB) src/libslapt.a
	-@echo "#ifndef LIB_SLAPT" > src/slapt.h
	-@echo "#define LIB_SLAPT 1" >> src/slapt.h
	-@cat $(LIBHEADERS) |grep -v '#include \"' >> src/slapt.h
	-@echo "#endif" >> src/slapt.h

test: libs
	(if [ -d t ]; then cd t; make runtest;fi)
check: test

testclean:
	-@(if [ -d t ]; then cd t; make clean;fi)

