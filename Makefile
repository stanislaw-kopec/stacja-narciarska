CC = gcc
CFLAGS = -Wall -std=c99 -Iinclude
SRC = src/main.c src/ipc_utils.c src/ski_station.c
OBJ = $(SRC:.c=.o)
TARGET = ski_station

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
