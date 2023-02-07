CXX	    = g++
CC      = gcc

CFLAGS	= -Wall -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
CXXFLAGS = $(CFLAGS)

TARGET = libdospath.a

all: libtest.exe

libtest.exe: libdospath.a libtest.o
	g++ $(CXXFLAGS) libtest.o libdospath.a -o libtest.exe

libdospath.a: dirtranslator.o filenameshortener.o libexports.o utilslib.o libcpptests.o
	ar rcs libdospath.a dirtranslator.o filenameshortener.o libexports.o libcpptests.o utilslib.o

lib: libdospath.a

clean:
	rm -f libtest *.o *.a *.exe
