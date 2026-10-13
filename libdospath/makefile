CXX	= g++
CC	= gcc

CFLAGS	= -Wall -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
CXXFLAGS	= $(CFLAGS)

TARGET	= libdospath.a

# unit tests - doctest framework, see ../third_party/doctest
TEST_DIR	= tests
TEST_SRCS	= $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJS	= $(TEST_SRCS:.cpp=.o)
TEST_BIN	= $(TEST_DIR)/run_tests.elf
TEST_CXXFLAGS	= $(CXXFLAGS) -I. -I../third_party/doctest

.PHONY: all lib test test-asan clean

all: libdospath.a

libdospath.a: dirtranslator.o filenameshortener.o libexports.o utilslib.o
	ar rcs libdospath.a dirtranslator.o filenameshortener.o libexports.o utilslib.o

lib: libdospath.a

$(TEST_DIR)/%.o: $(TEST_DIR)/%.cpp $(wildcard $(TEST_DIR)/*.h) *.h
	$(CXX) $(TEST_CXXFLAGS) -c $< -o $@

$(TEST_BIN): libdospath.a $(TEST_OBJS)
	$(CXX) $(TEST_CXXFLAGS) $(TEST_OBJS) libdospath.a -o $(TEST_BIN)

# build and run unit tests, pass extra doctest options via TEST_ARGS, e.g.: make test TEST_ARGS="-tc=*symlink* -s"
test: $(TEST_BIN)
	./$(TEST_BIN) $(TEST_ARGS)

# same tests, built with AddressSanitizer + UndefinedBehaviorSanitizer - fails on memory errors, leaks and UB
# (does a clean build, because the library objects need to be rebuilt with sanitizers too)
SANITIZE_FLAGS	= -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer

test-asan:
	$(MAKE) clean
	$(MAKE) test CFLAGS="$(CFLAGS) $(SANITIZE_FLAGS)"
	$(MAKE) clean

clean:
	rm -f libtest *.o *.a *.elf $(TEST_DIR)/*.o $(TEST_BIN)
