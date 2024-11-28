# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g

# Target executable
TARGET = emulator

# Source files
SRCS = main.c cpu_6502.c cpu_clock.c memory.c monitored.c bus.c queue.c event_queue.c

# Object files (generated from source files)
OBJS = $(SRCS:.c=.o)

# Include directories
INCLUDES = -I. -ID:/Code/c/lib/PDCurses

# Library directories
LIBDIRS = -LD:/Code/c/lib/PDCurses/build

# Libraries to link
LIBS = -l:pdcurses.a -lpthread  # Added -lpthread here

# Compilation rules
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LIBDIRS) $(LIBS)

# Compile .c files to .o files
%.o: %.c cpu_6502.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean rule to remove compiled files
clean:
	rm -f $(OBJS) $(TARGET).exe $(TARGET)

# Phony targets to avoid conflict with files named 'clean' or 'all'
.PHONY: clean all
