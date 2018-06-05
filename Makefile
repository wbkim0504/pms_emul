.SUFFIXES : .c .o

OBJS = pms_emul.o 
SRCS = $(OBJS:.o=.c)
TARGET = pms_emulator

CC = gcc
#CFLAGS = -g -c
CFLAGS = -c
LDFLAGS =
LDLIBS = -lpthread

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDLIBS)
	rm -f $(OBJS)

.c.o:
	$(CC) $(INC) $(CFLAGS) $<

clean:
	rm -f $(TARGET) $(OBJS)


