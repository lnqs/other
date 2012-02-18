CC = gcc
LD = ld
STRIP = sstrip
CFLAGS = -Wall -Os -std=c99 -fomit-frame-pointer -Wno-strict-aliasing
CPPFLAGS =
LDFLAGS = -dynamic-linker /lib/ld-linux.so.2
LIBS = -lc -lm -lasound

SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:.c=.o)
EXECUTABLE = other.bin
COMPRESSED = other

all: $(SOURCES) $(COMPRESSED)

$(COMPRESSED) : $(EXECUTABLE)
	echo '#!/bin/sh\nO=/tmp/o;dd if="$$0" bs=1 skip=71|unxz>$$O;chmod +x $$O;$$O;exit' > $@
	xz -c9 --format=lzma $< >> $@
	chmod +x $@

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
	rm -rf *.o *.d $(EXECUTABLE) $(COMPRESSED)

-include $(SOURCES:.c=.d)
