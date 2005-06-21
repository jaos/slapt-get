PROGRAM_NAME=slapt-get
VERSION=0.9.9m
ARCH=i386
RELEASE=1
CC=gcc
CURLFLAGS=`curl-config --libs`
OBJS=src/common.o src/configuration.o src/package.o src/curl.o src/transaction.o src/action.o src/main.o
LIBOBJS=src/common.o src/configuration.o src/package.o src/curl.o src/transaction.o
NONLIBOBJS=src/action.o src/main.o
RCDEST=/etc/slapt-get/slapt-getrc
RCSOURCE=example.slapt-getrc
LOCALESDIR=/usr/share/locale
SBINDIR=/usr/sbin/
DEFINES=-DPROGRAM_NAME="\"$(PROGRAM_NAME)\"" -DVERSION="\"$(VERSION)\"" -DRC_LOCATION="\"$(RCDEST)\"" -DENABLE_NLS -DLOCALESDIR="\"$(LOCALESDIR)\""
CFLAGS=-W -Werror -Wall -O2 -ansi -pedantic -Iinclude $(DEFINES) # add -fPIC for amd64

default: $(PROGRAM_NAME)

all: pkg

$(OBJS): 

$(PROGRAM_NAME): libs
	$(CC) -o $(PROGRAM_NAME) $(OBJS) $(CFLAGS) $(CURLFLAGS)

withlibslapt: libs
	$(CC) -o $(PROGRAM_NAME) $(NONLIBOBJS) $(CFLAGS) $(CURLFLAGS) -lslapt-$(VERSION)

static: libs
	$(CC) -o $(PROGRAM_NAME) $(OBJS) $(CFLAGS) $(CURLFLAGS) -static

install: $(PROGRAM_NAME) doinstall

staticinstall: static doinstall

withlibslaptinstall: withlibslapt doinstall

libsinstall: libs
	if [ ! -d $(DESTDIR)/usr/include ]; then mkdir -p $(DESTDIR)/usr/include;fi
	cp include/slapt.h $(DESTDIR)/usr/include/
	if [ ! -d $(DESTDIR)/usr/lib ]; then mkdir -p $(DESTDIR)/usr/lib;fi
	cp src/libslapt-$(VERSION).a src/libslapt-$(VERSION).so $(DESTDIR)/usr/lib/
	if [ -L $(DESTDIR)/usr/lib/libslapt.so ]; then rm $(DESTDIR)/usr/lib/libslapt.so;fi
	cd $(DESTDIR)/usr/lib; ln -s libslapt-$(VERSION).so libslapt.so
	if [ -L $(DESTDIR)/usr/lib/libslapt.a ]; then rm $(DESTDIR)/usr/lib/libslapt.a;fi
	cd $(DESTDIR)/usr/lib; ln -s libslapt-$(VERSION).a libslapt.a

doinstall: libsinstall
	strip --strip-unneeded $(PROGRAM_NAME)
	if [ ! -d $(DESTDIR)$(SBINDIR) ]; then mkdir -p $(DESTDIR)$(SBINDIR);fi
	install $(PROGRAM_NAME) $(DESTDIR)$(SBINDIR)
	chown root:bin $(DESTDIR)$(SBINDIR)$(PROGRAM_NAME)
	if [ ! -d $(DESTDIR)/etc/slapt-get ]; then mkdir -p $(DESTDIR)/etc/slapt-get;fi
	if [ -f $(DESTDIR)/etc/slapt-getrc ]; then mv $(DESTDIR)/etc/slapt-getrc $(DESTDIR)$(RCDEST);fi
	if [ ! -f $(DESTDIR)$(RCDEST) ]; then install --mode=0644 -b $(RCSOURCE) $(DESTDIR)$(RCDEST); else install --mode=0644 -b $(RCSOURCE) $(DESTDIR)$(RCDEST).new;fi
	if [ ! -d $(DESTDIR)/usr/man ]; then mkdir -p $(DESTDIR)/usr/man;fi
	if [ ! -d $(DESTDIR)/usr/man/man8 ]; then mkdir -p $(DESTDIR)/usr/man/man8;fi
	install $(PROGRAM_NAME).8 $(DESTDIR)/usr/man/man8/
	gzip -f $(DESTDIR)/usr/man/man8/$(PROGRAM_NAME).8
	install -d $(DESTDIR)/var/$(PROGRAM_NAME)
	for i in `ls po/ --ignore=slapt-get.pot --ignore=CVS |sed 's/.po//'` ;do if [ ! -d $(DESTDIR)$(LOCALESDIR)/$$i/LC_MESSAGES ]; then mkdir -p $(DESTDIR)$(LOCALESDIR)/$$i/LC_MESSAGES; fi; msgfmt -o $(DESTDIR)$(LOCALESDIR)/$$i/LC_MESSAGES/slapt-get.mo po/$$i.po;done
	mkdir -p $(DESTDIR)/usr/doc/$(PROGRAM_NAME)-$(VERSION)/
	cp example.slapt-getrc COPYING Changelog INSTALL README FAQ FAQ.html TODO $(DESTDIR)/usr/doc/$(PROGRAM_NAME)-$(VERSION)/

uninstall:
	-rm /$(SBINDIR)/$(PROGRAM_NAME)
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
	@mkdir -p pkg/$(SBINDIR)
	@mkdir -p pkg/etc/slapt-get
	@mkdir -p pkg/install
	@mkdir -p pkg/usr/man/man8
	@for i in `ls po/ --ignore=slapt-get.pot --ignore=CVS |sed 's/.po//'` ;do mkdir -p pkg$(LOCALESDIR)/$$i/LC_MESSAGES; msgfmt -o pkg$(LOCALESDIR)/$$i/LC_MESSAGES/slapt-get.mo po/$$i.po; done
	@cp $(PROGRAM_NAME) ./pkg/$(SBINDIR)
	@chown root:bin ./pkg/$(SBINDIR)
	@chown root:bin ./pkg/$(SBINDIR)/$(PROGRAM_NAME)
	@strip ./pkg/$(SBINDIR)/$(PROGRAM_NAME)
	@echo "# See /usr/doc/$(PROGRAM_NAME)-$(VERSION)/example.slapt-getrc " > ./pkg/etc/slapt-get/slapt-getrc.new
	@echo "# for example source entries and configuration hints." >> ./pkg/etc/slapt-get/slapt-getrc.new
	@cat example.slapt-getrc |grep -v '^#'|grep -v '^$$' >> ./pkg/etc/slapt-get/slapt-getrc.new
	@mkdir -p ./pkg/usr/doc/$(PROGRAM_NAME)-$(VERSION)/
	@cp example.slapt-getrc COPYING Changelog INSTALL README FAQ FAQ.html TODO ./pkg/usr/doc/$(PROGRAM_NAME)-$(VERSION)/
	@echo "if [ ! -d etc/slapt-get ]; then mkdir -p etc/slapt-get; fi; if [ -f etc/slapt-getrc -a ! -f etc/slapt-get/slapt-getrc ]; then mv etc/slapt-getrc etc/slapt-get/slapt-getrc; fi; if [ ! -f etc/slapt-get/slapt-getrc ]; then mv etc/slapt-get/slapt-getrc.new etc/slapt-get/slapt-getrc; else sed -re 's/(See \/usr\/doc\/slapt\-get\-).*(\/example\.slapt\-getrc)/\1$(VERSION)\2/' /etc/slapt-get/slapt-getrc > /tmp/tmp_slapt-getrc_tmp; cat /tmp/tmp_slapt-getrc_tmp > /etc/slapt-get/slapt-getrc; rm /tmp/tmp_slapt-getrc_tmp; diff -q etc/slapt-get/slapt-getrc etc/slapt-get/slapt-getrc.new >/dev/null 2>&1 && rm etc/slapt-get/slapt-getrc.new; fi;" > pkg/install/doinst.sh
	@cp slack-desc pkg/install/
	@cp slack-required pkg/install/
	@cp $(PROGRAM_NAME).8 pkg/usr/man/man8/
	@gzip pkg/usr/man/man8/$(PROGRAM_NAME).8
	@mkdir -p pkg/usr/lib
	@mkdir -p pkg/usr/include
	@cp include/slapt.h pkg/usr/include/
	@cp src/libslapt-$(VERSION).a src/libslapt-$(VERSION).so pkg/usr/lib/
	@strip pkg/usr/lib/libslapt-$(VERSION).so
	@( cd pkg/usr/lib; ln -s libslapt-$(VERSION).so libslapt.so; ln -s libslapt-$(VERSION).a libslapt.a )
	@( cd pkg; /sbin/makepkg -l y -c n $(PROGRAM_NAME)-$(VERSION)-$(ARCH)-$(RELEASE).tgz )

po_file:
	-grep '_(' src/*.c |cut -f2-255 -d':'|sed -re "s/.*(_\(\".*\"\)).*/\1/" > po/gettext_strings
	-mv po/slapt-get.pot po/slapt-get.pot~
	-xgettext -d slapt-get -o po/slapt-get.pot -a -C --no-location po/gettext_strings
	-rm po/gettext_strings

libs: CFLAGS += -fpic #change to -fPIC for amd64
libs: $(OBJS)
	touch libs
	$(CC) -shared -o src/libslapt-$(VERSION).so $(LIBOBJS)
	ar -r src/libslapt-$(VERSION).a $(LIBOBJS)
	cat include/main.h include/common.h include/configuration.h include/package.h include/curl.h include/transaction.h |grep -v '#include \"' > include/slapt.h

