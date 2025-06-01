CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -DNDEBUG
DEBUG_CFLAGS = -Wall -Wextra -std=c11 -g -DDEBUG
LDFLAGS = -lcurl -ljson-c -lpthread

# Directories
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
TEST_DIR = tests

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TARGET = $(BIN_DIR)/cpm

# Test files
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c)
TEST_OBJECTS = $(TEST_SOURCES:$(TEST_DIR)/%.c=$(BUILD_DIR)/test_%.o)
TEST_TARGET = $(BUILD_DIR)/test_runner

.PHONY: all clean debug test install

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	chmod +x $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

debug: CFLAGS = $(DEBUG_CFLAGS)
debug: $(TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJECTS) $(filter-out $(BUILD_DIR)/main.o, $(OBJECTS))
	$(CC) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/test_%.o: $(TEST_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -c $< -o $@

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/cpm

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.SUFFIXES:
.SUFFIXES: .c .o