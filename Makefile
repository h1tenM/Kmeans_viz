CC = gcc
CFLAGS = -g -Wall -Wextra -Iinclude
LDFLAGS = -Llib
LDLIBS = -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo

# Directories
SRC_DIR = src
OBJ_DIR = build
BIN_DIR = bin

# Files
# Replace 'main' with the actual name of your file (e.g., kmeans2)
TARGET = $(BIN_DIR)/kmeans_viz
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES))

# Default target
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS) $(LDLIBS)

# Compile source files to object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create directories if they don't exist
$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

# Clean up
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all clean