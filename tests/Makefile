# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g

# Target executable
TARGET = test

# Source files
SRCS = test.c ../cpu_6502.c ../cpu_clock.c ../memory.c ../bus.c ../queue.c

# Object files (generated from source files)
OBJS = $(SRCS:.c=.o)

# Include directories
INCLUDES = -I. -ID:/Code/c/lib/PDCurses -I../

# Library directories
LIBDIRS = -LD:/Code/c/lib/PDCurses/build

# Libraries to link
LIBS = -l:pdcurses.a -lpthread  # Added -lpthread here

# Compilation rules
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LIBDIRS) $(LIBS)

# Compile .c files to .o files
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean rule to remove compiled files
clean:
	rm -f $(OBJS) $(TARGET).exe $(TARGET)

# Phony targets to avoid conflict with files named 'clean' or 'all'
.PHONY: clean all
