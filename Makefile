CC ?= cc

CFLAGS ?= -std=c17 -Wall -Wextra -pedantic -O2
CPPFLAGS ?= -Isrc
LDFLAGS ?=
LDLIBS ?= -lncurses

TARGET = pc98fm
SOURCES = src/main.c src/core.c src/archive_zip.c src/fs_posix.c src/ui_curses.c \
          src/third_party/miniz.c src/third_party/miniz_tdef.c \
          src/third_party/miniz_tinfl.c src/third_party/miniz_zip.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(SOURCES) $(LDLIBS) -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
