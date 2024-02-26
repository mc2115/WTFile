CC=gcc
CFLAGS=-I. -g
LIBS=-lpthread
DEPS = ./include/Utils.h ./include/Pack.h  ./include/SockUtils.h ./include/VectorStr.h ./include/Manifest.h ./include/MessageType.h

%.o: src/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: WTF WTFServer

WTF: WTF.o
	$(CC) -o WTF WTF.o

WTFServer: WTFServer.o
	$(CC) -o WTFServer WTFServer.o $(LIBS)

WTFTest: WTFTest.o
	$(CC) -o WTFTest WTFTest.o $(LIBS)

test: all WTFTest
	./WTFTest testplan.txt

clean:
	rm -f WTF.o WTF WTFServer.o WTFServer WTFTest.o WTFTest
