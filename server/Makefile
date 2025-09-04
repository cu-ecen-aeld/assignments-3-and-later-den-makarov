SRC := aesdsocket.c
TARGET = aesdsocket
OBJS := $(SRC:.c=.o)

all: $(TARGET)

CC := $(CROSS_COMPILE)gcc

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	-rm -f *.o $(TARGET) *.elf *.map
