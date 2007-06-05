TESTOBJS=test.o test_common.o test_configuration.o test_curl.o test_packages.o test_transaction.o
CC=gcc
CURLFLAGS=`curl-config --libs`
DEFINES=
CFLAGS=-W -Werror -Wall -O2 -ansi -pedantic $(DEFINES)
LDFLAGS=$(CURLFLAGS) -lz
CHECKFLAGS=`pkg-config --libs check`

test: $(TESTOBJS)
	$(CC) -o test $(TESTOBJS) $(CFLAGS) ../src/libslapt.so $(CHECKFLAGS) $(LDFLAGS)

runtest: test
	./test

$(TESTOBJS):

clean:
	-rm *.o test
	-rm -r data/slapt-get
