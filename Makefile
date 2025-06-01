# CPM Makefile - Alternative build system to CMake
# Author: Dr. Q Josef Kurk Edwards

# Project configuration
PROJECT = cpm
VERSION = 0.1.0-alpha
PREFIX ?= /usr/local

# Compiler configuration
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -Iinclude
LDFLAGS = -lpmem -lpmemobj -lcjson -lpthread -lm

# Debug/Release configurations
DEBUG_CFLAGS = $(CFLAGS) -g -O0 -DDEBUG
RELEASE_CFLAGS = $(CFLAGS) -O3 -DNDEBUG

# Directories
SRCDIR = lib
INCDIR = include
TESTDIR = test
BUILDDIR = build
BINDIR = bin

# Source files
LIB_SOURCES = $(shell find $(SRCDIR) -name '*.c')
LIB_OBJECTS = $(LIB_SOURCES:%.c=$(BUILDDIR)/%.o)

MAIN_SOURCE = cpm.c
CPX_SOURCE = cpx.c

TEST_SOURCES = $(shell find $(TESTDIR) -name '*.c')
TEST_OBJECTS = $(TEST_SOURCES:%.c=$(BUILDDIR)/%.o)

# Targets
STATIC_LIB = $(BUILDDIR)/libcpm.a
SHARED_LIB = $(BUILDDIR)/libcpm.so
CPM_BIN = $(BINDIR)/cpm
CPX_BIN = $(BINDIR)/cpx

# Default target
.PHONY: all
all: debug

# Debug build
.PHONY: debug
debug: CFLAGS = $(DEBUG_CFLAGS)
debug: $(CPM_BIN) $(CPX_BIN) $(STATIC_LIB) $(SHARED_LIB)

# Release build
.PHONY: release
release: CFLAGS = $(RELEASE_CFLAGS)
release: $(CPM_BIN) $(CPX_BIN) $(STATIC_LIB) $(SHARED_LIB)

# Create directories
$(BUILDDIR) $(BINDIR):
	@mkdir -p $@

# Object files
$(BUILDDIR)/%.o: %.c | $(BUILDDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Static library
$(STATIC_LIB): $(LIB_OBJECTS) | $(BUILDDIR)
	ar rcs $@ $^

# Shared library
$(SHARED_LIB): $(LIB_OBJECTS) | $(BUILDDIR)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

# Main executable
$(CPM_BIN): $(MAIN_SOURCE) $(STATIC_LIB) | $(BINDIR)
	$(CC) $(CFLAGS) $< -L$(BUILDDIR) -lcpm $(LDFLAGS) -o $@

# CPX executable
$(CPX_BIN): $(CPX_SOURCE) $(STATIC_LIB) | $(BINDIR)
	$(CC) $(CFLAGS) $< -L$(BUILDDIR) -lcpm $(LDFLAGS) -o $@

# Tests
.PHONY: test
test: $(TEST_OBJECTS) $(STATIC_LIB)
	@echo "Building and running tests..."
	@for test_obj in $(TEST_OBJECTS); do \
		test_name=$$(basename $$test_obj .o); \
		test_bin=$(BUILDDIR)/$$test_name; \
		$(CC) $(CFLAGS) $$test_obj -L$(BUILDDIR) -lcpm $(LDFLAGS) -o $$test_bin; \
		echo "Running $$test_name..."; \
		$$test_bin || exit 1; \
	done

# Install
.PHONY: install
install: release
	@echo "Installing CPM to $(PREFIX)..."
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/include/cpm
	install -d $(DESTDIR)$(PREFIX)/share/cpm
	install -m 755 $(CPM_BIN) $(DESTDIR)$(PREFIX)/bin/
	install -m 755 $(CPX_BIN) $(DESTDIR)$(PREFIX)/bin/
	install -m 644 $(STATIC_LIB) $(DESTDIR)$(PREFIX)/lib/
	install -m 755 $(SHARED_LIB) $(DESTDIR)$(PREFIX)/lib/
	install -m 644 $(INCDIR)/*.h $(DESTDIR)$(PREFIX)/include/cpm/
	install -m 644 cpm_package.spec $(DESTDIR)$(PREFIX)/share/cpm/

# Uninstall
.PHONY: uninstall
uninstall:
	@echo "Uninstalling CPM from $(PREFIX)..."
	rm -f $(DESTDIR)$(PREFIX)/bin/cpm
	rm -f $(DESTDIR)$(PREFIX)/bin/cpx
	rm -f $(DESTDIR)$(PREFIX)/lib/libcpm.a
	rm -f $(DESTDIR)$(PREFIX)/lib/libcpm.so
	rm -rf $(DESTDIR)$(PREFIX)/include/cpm
	rm -rf $(DESTDIR)$(PREFIX)/share/cpm

# Clean
.PHONY: clean
clean:
	rm -rf $(BUILDDIR) $(BINDIR)

# Format code
.PHONY: format
format:
	@echo "Formatting code..."
	find $(SRCDIR) $(INCDIR) -name '*.c' -o -name '*.h' | xargs clang-format -i

# Lint code
.PHONY: lint
lint:
	@echo "Linting code..."
	cppcheck --enable=all --error-exitcode=1 $(SRCDIR) $(INCDIR)

# Generate documentation
.PHONY: docs
docs:
	@echo "Generating documentation..."
	doxygen docs/Doxyfile

# Package
.PHONY: package
package: release
	@echo "Creating package..."
	tar -czf $(PROJECT)-$(VERSION).tar.gz \
		--exclude='.git*' \
		--exclude='build' \
		--exclude='*.tar.gz' \
		.

# Help
.PHONY: help
help:
	@echo "CPM Build System"
	@echo "Available targets:"
	@echo "  all      - Build everything (debug mode)"
	@echo "  debug    - Build debug version"
	@echo "  release  - Build release version"
	@echo "  test     - Build and run tests"
	@echo "  install  - Install to system (requires sudo)"
	@echo "  uninstall- Remove from system (requires sudo)"
	@echo "  clean    - Clean build artifacts"
	@echo "  format   - Format source code"
	@echo "  lint     - Lint source code"
	@echo "  docs     - Generate documentation"
	@echo "  package  - Create source package"
	@echo "  help     - Show this help"

# Dependencies
-include $(LIB_OBJECTS:.o=.d)
-include $(TEST_OBJECTS:.o=.d)