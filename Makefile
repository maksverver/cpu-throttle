CFLAGS=-std=c99 -Wall -O2
LDLIBS=-lcpupower
BIN=cpu-throttle
OBJ=main.o number-to-string.o parse-frequency.o

all: cpu-throttle

cpu-throttle: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

clean:
	rm -f $(OBJ)

distclean: clean
	rm -f $(BIN)

.PHONY: all clean distclean
