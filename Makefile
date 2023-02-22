TARGET = demo

CC = gcc
CPPFLAGS += -pthread -I./deps/cglm/include
LDFLAGS += -pthread
LDLIBS += -lm -lhidapi-libusb -lglfw -lGL -lGLEW
SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:src/%.c=src/%.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

src/%.o: src/%.c

clean:
	$(RM) $(TARGET) src/*.o
