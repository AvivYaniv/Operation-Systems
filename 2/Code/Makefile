CC= gcc
CFLAGS = -Wall -O3 -std=gnu99
TARGET = main
RM = rm
SOURCES = myshell.c os1819a_hw2.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean

all: $(TARGET)

clean:
	$(RM) -f $(TARGET) $(OBJECTS)

$(TARGET) : $(OBJECTS)
	$(CC) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $<
