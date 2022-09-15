all: libtest.exe

libtest.exe: libdospath.a libtest.o
	g++ libtest.o libdospath.a -o libtest.exe

libdospath.a: dirtranslator.o filenameshortener.o libexports.o utilslib.o libcpptests.o
	ar rcs libdospath.a dirtranslator.o filenameshortener.o libexports.o libcpptests.o utilslib.o

lib: libdospath.a

clean:
	rm -f libtest *.o *.a *.exe
