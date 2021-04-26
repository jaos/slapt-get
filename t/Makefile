TESTOBJS=test.o test_common.o test_configuration.o test_curl.o test_packages.o test_transaction.o
CC=gcc
CURLFLAGS=`curl-config --libs`
GPGMEFLAGS=`gpgme-config --libs`
DEFINES=-DSLAPTTESTDIR=$(shell pwd) -DSLAPT_HAS_GPGME
CFLAGS=-W -Werror -Wall -O2 -pedantic $(DEFINES)
LDFLAGS=$(CURLFLAGS) $(GPGMEFLAGS) -lz -lm
CHECKFLAGS=`pkg-config --libs check`

test: $(TESTOBJS)
	$(CC) -o test $(TESTOBJS) $(CFLAGS) -Wl,-rpath,../src/ -L../src/ -lslapt $(CHECKFLAGS) $(LDFLAGS)

runtest: test
	sed -i.bak -re 's|(WORKINGDIR=)([^\/]+.*)|\1$(shell pwd)/\2|' data/rc1
	./test || (mv -f data/rc1.bak data/rc1 || exit 1)
	mv -f data/rc1.bak data/rc1

$(TESTOBJS):

clean:
	-rm *.o test
	-rm -r data/slapt-get
