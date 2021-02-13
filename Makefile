PACKAGE=slapt-get
VERSION=0.11.1
ARCH?=$(shell gcc -dumpmachine | cut -f1 -d- | sed -e "s/i[3456]86/i386/")
LIBDIR=$(shell dirname $$(gcc -print-file-name=libcrypto.so))
RELEASE=1
CC?=gcc
STRIP?=strip
RANLIB?=ranlib
CURLFLAGS=`curl-config --libs` -lcrypto
OBJS=src/common.o src/configuration.o src/package.o src/slaptcurl.o src/transaction.o src/action.o src/main.o
LIBOBJS=src/common.o src/configuration.o src/package.o src/slaptcurl.o src/transaction.o
LIBHEADERS=src/main.h src/common.h src/configuration.h src/package.h src/slaptcurl.h src/transaction.h
NONLIBOBJS=src/action.o src/main.o
RCDEST=/etc/slapt-get/slapt-getrc
RCSOURCE=default.slapt-getrc.$(ARCH)
PACKAGE_LOCALE_DIR=/usr/share/locale
SBINDIR=/usr/sbin/
GETTEXT_PACKAGE=$(PACKAGE)
DEFINES=-DPACKAGE="\"$(PACKAGE)\"" -DVERSION="\"$(VERSION)\"" -DRC_LOCATION="\"$(RCDEST)\"" -DENABLE_NLS -DPACKAGE_LOCALE_DIR="\"$(PACKAGE_LOCALE_DIR)\"" -DGETTEXT_PACKAGE="\"$(GETTEXT_PACKAGE)\""
LDFLAGS+=$(CURLFLAGS) -lz -lm -flto=auto
HAS_GPGME=$(shell gpgme-config --libs 2>&1 >/dev/null && echo 1)
ifeq ($(HAS_GPGME),1)
	DEFINES+=-DSLAPT_HAS_GPGME
	OBJS+=src/slaptgpgme.o
	LIBOBJS+=src/slaptgpgme.o
	LIBHEADERS+=src/slaptgpgme.h
	LDFLAGS+=`gpgme-config --libs`
endif
ifeq ($(TESTBUILD),1)
CFLAGS?=-W -Werror -Wall -Wextra -O2 -flto -ffat-lto-objects -pedantic -Wshadow -Wstrict-overflow -fno-strict-aliasing -g -fsanitize=undefined -fsanitize=address -fstack-protector -ggdb -fno-omit-frame-pointer
else
CFLAGS?=-W -Werror -Wall -Wextra -O2 -flto -ffat-lto-objects -pedantic -Wshadow -Wstrict-overflow -fno-strict-aliasing -g
endif
CFLAGS+=$(DEFINES) -fPIC

default: $(PACKAGE)

all: pkg

$(OBJS): $(LIBHEADERS)

$(PACKAGE): libs
ifeq ($(TESTBUILD),1)
	$(CC) -o $(PACKAGE) $(OBJS) $(CFLAGS) $(LDFLAGS)
else
	$(CC) -o $(PACKAGE) $(NONLIBOBJS) -L./src -Wl,-R$(LIBDIR) $(CFLAGS) $(LDFLAGS) -lslapt
endif

install: $(PACKAGE) doinstall

libsinstall: libs
	if [ ! -d $(DESTDIR)/usr/include ]; then mkdir -p $(DESTDIR)/usr/include;fi
	cp -f src/slapt.h $(DESTDIR)/usr/include/
	if [ ! -d $(DESTDIR)$(LIBDIR) ]; then mkdir -p $(DESTDIR)$(LIBDIR);fi
	if [ -L $(DESTDIR)$(LIBDIR)/libslapt.so ]; then rm $(DESTDIR)$(LIBDIR)/libslapt.so;fi
	cp -f src/libslapt.so.$(VERSION) $(DESTDIR)$(LIBDIR)/
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
	install -d $(DESTDIR)/var/cache/$(PACKAGE)
	for i in `ls po/*.po | sed 's/.po//' | xargs -n1 basename` ;do if [ ! -d $(DESTDIR)$(PACKAGE_LOCALE_DIR)/$$i/LC_MESSAGES ]; then mkdir -p $(DESTDIR)$(PACKAGE_LOCALE_DIR)/$$i/LC_MESSAGES; fi; msgfmt -o $(DESTDIR)$(PACKAGE_LOCALE_DIR)/$$i/LC_MESSAGES/slapt-get.mo po/$$i.po;done
	mkdir -p $(DESTDIR)/usr/doc/$(PACKAGE)-$(VERSION)/
	cp -f default.slapt-getrc.* example.slapt-getrc COPYING ChangeLog INSTALL README FAQ TODO $(DESTDIR)/usr/doc/$(PACKAGE)-$(VERSION)/

uninstall:
	-rm /$(SBINDIR)/$(PACKAGE)
	-@echo leaving $(RCDEST)
	-rm /usr/man/man8/$(PACKAGE).8.gz
	-rm /usr/man/man3/libslapt.3.gz
	-rm /usr/man/ru/man8/$(PACKAGE).8.gz
	-rm /usr/man/uk/man8/$(PACKAGE).8.gz
	-@echo leaving /var/cache/$(PACKAGE)
	-rm -r /usr/doc/$(PACKAGE)-$(VERSION)
	-find /usr/share/locale/ -name 'slapt-get.mo' -exec rm {} \;
	-rm /usr/include/slapt.h
	-readlink $(LIBDIR)/libslapt.so|xargs -r rm
	-rm $(LIBDIR)/libslapt.so

clean: testclean
	-if [ -f $(PACKAGE) ]; then rm $(PACKAGE);fi
	-rm src/*.o
	-rm src/*.so
	-rm src/libslapt.so*
	-rm src/slapt.h
	-rm slapt-get-*.txz
	-if [ -d pkg ]; then rm -rf pkg ;fi
	-if [ -f libs ]; then rm -rf libs ;fi


.PHONY: pkg dopkg
pkg: $(PACKAGE) dopkg

dopkg: $(PACKAGE)
	mkdir -p pkg
	mkdir -p pkg/$(SBINDIR)
	mkdir -p pkg/etc/slapt-get
	mkdir -p pkg/install
	mkdir -p pkg/usr/man/man8
	mkdir -p pkg/usr/man/man3
	mkdir -p pkg/usr/man/ru/man8
	mkdir -p pkg/usr/man/uk/man8
	for i in `ls po/ --ignore=slapt-get.pot --ignore='*~*' |sed 's/.po//'` ;do mkdir -p pkg$(PACKAGE_LOCALE_DIR)/$$i/LC_MESSAGES; msgfmt -o pkg$(PACKAGE_LOCALE_DIR)/$$i/LC_MESSAGES/slapt-get.mo po/$$i.po; done
	cp -f $(PACKAGE) ./pkg/$(SBINDIR)
	chown $$(stat --format "%u:%g" /usr/sbin) ./pkg/$(SBINDIR)
	chown $$(stat --format "%u:%g" /usr/sbin) ./pkg/$(SBINDIR)/$(PACKAGE)
	$(STRIP) ./pkg/$(SBINDIR)/$(PACKAGE)
	cp -f $(RCSOURCE) pkg/etc/slapt-get/slapt-getrc.new
	mkdir -p ./pkg/usr/doc/$(PACKAGE)-$(VERSION)/
	cp -f default.slapt-getrc.* example.slapt-getrc COPYING ChangeLog INSTALL README FAQ TODO ./pkg/usr/doc/$(PACKAGE)-$(VERSION)/
	echo "if [ ! -d etc/slapt-get ]; then mkdir -p etc/slapt-get; fi; if [ -f etc/slapt-getrc -a ! -f etc/slapt-get/slapt-getrc ]; then mv -f etc/slapt-getrc etc/slapt-get/slapt-getrc; fi; if [ ! -f etc/slapt-get/slapt-getrc ]; then mv -f etc/slapt-get/slapt-getrc.new etc/slapt-get/slapt-getrc; else cmp etc/slapt-get/slapt-getrc etc/slapt-get/slapt-getrc.new >/dev/null 2>&1 && rm etc/slapt-get/slapt-getrc.new; fi;" > pkg/install/doinst.sh
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
	cp -f src/libslapt.so.$(VERSION) pkg$(LIBDIR)/
	$(STRIP) pkg$(LIBDIR)/libslapt.so.$(VERSION)
	( cd pkg$(LIBDIR); ln -sf libslapt.so.$(VERSION) libslapt.so )
	( cd pkg; /sbin/makepkg -l y -c n ../$(PACKAGE)-$(VERSION)-$(ARCH)-$(RELEASE).txz )

po_file:
	-xgettext -o po/slapt-get.pot.new -sC --no-location src/*.c src/*.h
	-if [ ! -f po/slapt-get.pot ]; then mv po/slapt-get.pot.new po/slapt-get.pot; else msgmerge --no-wrap -Us po/slapt-get.pot po/slapt-get.pot.new ; fi
	-rm po/slapt-get.pot.new
	-for i in `ls po/*.po`; do msgmerge --no-wrap -UNs $$i po/slapt-get.pot; done

libs: $(OBJS)
	touch libs
	$(CC) -shared -o src/libslapt.so.$(VERSION) $(LIBOBJS) -Wl,-soname=libslapt.so.$(VERSION) $(CFLAGS) $(LDFLAGS)
	( cd src; if [ -f libslapt.so ]; then rm libslapt.so;fi; ln -sf libslapt.so.$(VERSION) libslapt.so )
	-@echo "#ifndef LIB_SLAPT" > src/slapt.h
	-@echo "#define LIB_SLAPT 1" >> src/slapt.h
	-@cat $(LIBHEADERS) |grep -v '#include \"' >> src/slapt.h
	-@echo "#endif" >> src/slapt.h

test: libs
	(if [ -d t ]; then cd t; make runtest;fi)
check: test

testclean:
	-@(if [ -d t ]; then cd t; make clean;fi)

dist:
	git archive --format=tar --prefix=slapt-get-$(VERSION)/ HEAD | gzip -9 > slapt-get-$(VERSION).tar.gz
	md5sum slapt-get-$(VERSION).tar.gz > slapt-get-$(VERSION).md5sum
