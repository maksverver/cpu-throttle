CFLAGS=-std=c99 -Wall -O2
LDLIBS=-lcpupower
BIN=cpu-throttle
OBJ=main.o number-to-string.o parse-frequency.o

TESTS=test_number-to-string test_parse-frequency
TEST_BIN=number-to-string_test parse-frequency_test

all: $(BIN)

cpu-throttle: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

number-to-string_test: number-to-string.o number-to-string_test.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ number-to-string.o number-to-string_test.o

parse-frequency_test: parse-frequency.o parse-frequency_test.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ parse-frequency.o parse-frequency_test.o

test_number-to-string: number-to-string_test; ./number-to-string_test
test_parse-frequency: parse-frequency_test; ./parse-frequency_test

test: $(TESTS)

clean:
	rm -f ./*.o $(TEST_BIN)

distclean: clean
	rm -f $(BIN)

.PHONY: all clean distclean test $(TESTS)
