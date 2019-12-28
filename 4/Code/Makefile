CC= gcc
CFLAGS = -Wall -O3 -std=gnu99 -pthread
TARGET = distributed_search
RM = rm
SOURCES = distributed_search.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean

all: $(TARGET)

clean:
	$(RM) -f $(TARGET) $(OBJECTS)

$(TARGET) : $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $<
