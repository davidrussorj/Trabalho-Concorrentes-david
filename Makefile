CC=gcc
CFLAGS=-O3 -Wall -Wextra -pthread
TARGET=image_threads
SRC=src/image_threads.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run-a: $(TARGET)
	./$(TARGET) 4 1

run-b: $(TARGET)
	./$(TARGET) 4 2

run-c: $(TARGET)
	./$(TARGET) 4 3 64 64

clean:
	rm -f $(TARGET)
