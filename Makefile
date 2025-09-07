CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
LDFLAGS = -lraylib -lm -lpthread -ldl -lrt -lX11

# Detect OS
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LDFLAGS += -lGL
endif
ifeq ($(UNAME_S),Darwin)
    LDFLAGS = -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL -lraylib
endif

TARGET = wfc
SOURCE = wfc.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

.PHONY: all clean run debug
