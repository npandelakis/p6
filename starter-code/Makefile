CC=gcc
CFLAGS=-ggdb3 -c -Wall -Werror -std=gnu99
LDFLAGS=-pthread
SOURCES=proxyserver.c safequeue.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=proxyserver

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) -g $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) -g $(CFLAGS) $< -o $@

clean:
	rm -f $(EXECUTABLE) $(OBJECTS)
