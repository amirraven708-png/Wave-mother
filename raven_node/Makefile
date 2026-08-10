CC = gcc
CFLAGS = -Wall -O2 -I$(PREFIX)/include
LDFLAGS = -L$(PREFIX)/lib -lssl -lcrypto -lm
SRC = src/main.c src/crypto_utils.c src/wave_engine.c src/node.c src/blockchain.c src/crosschain.c src/wave_mother_bridge.c
TARGET = raven_node

all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
