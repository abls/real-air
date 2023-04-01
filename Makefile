TARGET = demo

CC = gcc
CPPFLAGS += -pthread -I./deps/cglm/include -I./deps/Fusion
LDFLAGS += -pthread
LDLIBS += -lm -lhidapi-libusb -lglfw -lGL -lGLEW
SRCS = $(wildcard src/*.c)
FUSS = $(wildcard deps/Fusion/Fusion/*.c)
OBJS = $(SRCS:src/%.c=src/%.o) $(FUSS:deps/Fusion/Fusion/%.c=deps/Fusion/Fusion/%.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

src/%.o: src/%.c

clean:
	$(RM) $(TARGET) src/*.o
