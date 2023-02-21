CC = gcc
SRCS = main.c tracking.c
OBJS = $(SRCS:.c=.o)
INCS = -I./cglm/include
LIBS = -lm -lhidapi-libusb -lglfw -lGL -lGLEW
TARGET = demo

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(INCS) -c -o $@ $<

clean:
	$(RM) $(TARGET) *.o
