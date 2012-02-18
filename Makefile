CC = gcc
LD = ld
STRIP = sstrip
CFLAGS = -Wall -Os -std=c99 -fomit-frame-pointer -Wno-strict-aliasing
CPPFLAGS =
LDFLAGS = -dynamic-linker /lib/ld-linux.so.2
LIBS = -lc -lm -lasound

SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:.c=.o)
EXECUTABLE = other

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)
	$(STRIP) $@

%.o: %.c %.d
	$(CC) -c $(CFLAGS) $< -o $@

%.d: %.c
	@$(SHELL) -ec '$(CC) -M $(CPPFLAGS) $< \
		| sed '\''s/\($*\)\.o[ :]*/\1.o $@ : /g'\'' > $@; \
		[ -s $@ ] || rm -f $@'

clean:
	rm -rf *.o *.d $(EXECUTABLE)

-include $(SOURCES:.c=.d)
