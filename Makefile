CFLAGS=-std=c99 -Wall -O2
LDLIBS=-lcpupower
BIN=cpu-throttle
OBJ=main.o number-to-string.o parse-frequency.o

TESTS=test_number-to-string
TEST_BIN=number-to-string_test

all: $(BIN)

cpu-throttle: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

number-to-string_test: number-to-string.o number-to-string_test.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ number-to-string.o number-to-string_test.o

test_number-to-string: number-to-string_test; ./number-to-string_test

test: $(TESTS)

clean:
	rm -f ./*.o

distclean: clean
	rm -f $(BIN) $(TEST_BIN)

.PHONY: all clean distclean test $(TESTS)
