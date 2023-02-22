TARGET = demo

CC = gcc
SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:src/%.c=src/%.o)
INCS = -I./deps/cglm/include
LIBS = -lm -lhidapi-libusb -lglfw -lGL -lGLEW

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)

src/%.o: src/%.c
	$(CC) $(INCS) -c -o $@ $<

clean:
	$(RM) $(TARGET) src/*.o
