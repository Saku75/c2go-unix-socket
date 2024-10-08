# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -pthread

# Linker flags
LDFLAGS = -lm

# Target executable
TARGET = ./dist/prime-calculator

# Source files
SRCS = $(wildcard ./src/*.c)

# Object files
OBJS = $(SRCS:.c=.o)

# Create the dist directory if it doesn't exist and build the target executable
$(TARGET): $(OBJS)
	mkdir -p ./dist
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)
	rm -f $(OBJS)

# Compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule to remove compiled files
clean:
	rm -f $(OBJS) $(TARGET)