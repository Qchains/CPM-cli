# Makefile for CPM (C Package Manager)
# Author: Dr. Q Josef Kurk Edwards

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -O2
LDFLAGS = -lpthread -lcurl

# Directories
SRCDIR = lib
INCDIR = include
BUILDDIR = build
BINDIR = bin

# Source files
CORE_SOURCES = $(wildcard $(SRCDIR)/core/*.c)
CLI_SOURCES = $(wildcard $(SRCDIR)/cli/*.c)
COMMAND_SOURCES = $(wildcard $(SRCDIR)/commands/*.c)
MAIN_SOURCE = cpm.c
LIB_SOURCES = lib/core/cpm_package.c lib/core/cpm_pmll.c lib/core/cpm_promise.c \
              lib/commands/install.c lib/commands/publish.c lib/commands/search.c \
              lib/commands/init.c lib/commands/help.c lib/commands/run_script.c

# Object files
CORE_OBJECTS = $(CORE_SOURCES:$(SRCDIR)/core/%.c=$(BUILDDIR)/core/%.o)
CLI_OBJECTS = $(CLI_SOURCES:$(SRCDIR)/cli/%.c=$(BUILDDIR)/cli/%.o)
COMMAND_OBJECTS = $(COMMAND_SOURCES:$(SRCDIR)/commands/%.c=$(BUILDDIR)/commands/%.o)
MAIN_OBJECT = $(BUILDDIR)/cpm.o

ALL_OBJECTS = $(CORE_OBJECTS) $(CLI_OBJECTS) $(COMMAND_OBJECTS) $(MAIN_OBJECT)

# Target executable
TARGET = $(BINDIR)/cpm

# Default target
all: directories $(TARGET)

# Create necessary directories
directories:
        @mkdir -p $(BUILDDIR)/core
        @mkdir -p $(BUILDDIR)/cli
        @mkdir -p $(BUILDDIR)/commands
        @mkdir -p $(BINDIR)

# Main target
$(TARGET): $(ALL_OBJECTS)
        $(CC) $(ALL_OBJECTS) -o $@ $(LDFLAGS)

# Main object
$(BUILDDIR)/cpm.o: $(MAIN_SOURCE)
        $(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

# Core objects
$(BUILDDIR)/core/%.o: $(SRCDIR)/core/%.c
        $(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

# CLI objects
$(BUILDDIR)/cli/%.o: $(SRCDIR)/cli/%.c
        $(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

# Command objects
$(BUILDDIR)/commands/%.o: $(SRCDIR)/commands/%.c
        $(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

# Clean build files
clean:
        rm -rf $(BUILDDIR) $(BINDIR)

# Install (copy to system path)
install: $(TARGET)
        cp $(TARGET) /usr/local/bin/

# Uninstall
uninstall:
        rm -f /usr/local/bin/cpm

# Test build
test: $(TARGET)
        ./$(TARGET) help

.PHONY: all clean install uninstall test directories