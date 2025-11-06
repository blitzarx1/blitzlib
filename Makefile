CC := cc
CFLAGS := -Wall -Wextra -std=c99

# Vectoring optimization flags
CFLAGS += -Ofast -ffast-math -funroll-loops -march=native -ftree-vectorize -Rpass=loop-vectorize

LIBSQLITE_DIR := deps/sqlite-autoconf-3500400
LIBSQLITE_A := $(LIBSQLITE_DIR)/libsqlite3.a

LIBRAYLIB_DIR := deps/raylib-5.5_macos/
LIBRAYLIB_A := $(LIBRAYLIB_DIR)/lib/libraylib.a

INCLUDES := -I$(LIBSQLITE_DIR) -I$(LIBRAYLIB_DIR)/include

# macOS frameworks required by raylib/GLFW
FRAMEWORKS := \
	-framework Cocoa \
	-framework IOKit \
	-framework CoreVideo \
	-framework OpenGL \
	-framework CoreGraphics \
	-framework CoreFoundation

.PHONY: all run clean

all: main

main: main.c
	$(CC) $(CFLAGS) $(INCLUDES) -o main main.c $(LIBSQLITE_A) $(LIBRAYLIB_A) $(FRAMEWORKS)

run: main
	./main

clean:
	rm -f main
