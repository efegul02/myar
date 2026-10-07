CC     ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
SRC     = src/main.c src/archive.c
BIN     = myar

all: $(BIN)

$(BIN): $(SRC) src/archive.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

test: $(BIN)
	sh tests/run_tests.sh ./$(BIN)

# Debug build + Valgrind (Linux / WSL; needs `sudo apt install valgrind`)
valgrind:
	$(CC) -std=c11 -g -O0 -Wall -Wextra -Wpedantic -o $(BIN)-debug $(SRC)
	sh tests/valgrind.sh ./$(BIN)-debug

clean:
	rm -f $(BIN) $(BIN)-debug

.PHONY: all test valgrind clean
