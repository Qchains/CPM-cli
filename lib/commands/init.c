/*
 * File: init.c
 * Description: CPM init command implementation - npm init equivalent for C packages
 * Author: Dr. Q Josef Kurk Edwards
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include "cpm.h"
#include "cpm_package.h"

// --- Input Helper Functions ---
static void read_input(const char* prompt, const char* default_value, char* buffer, size_t buffer_size) {
    printf("%s", prompt);
    if (default_value && strlen(default_value) > 0) {
        printf(" [%s]", default_value);
    }
    printf(": ");
    
    if (fgets(buffer, buffer_size, stdin)) {
        // Remove newline
        buffer[strcspn(buffer, "\n")] = 0;
        
        // Use default if empty input
        if (strlen(buffer) == 0 && default_value) {
            strncpy(buffer, default_value, buffer_size - 1);
            buffer[buffer_size - 1] = 0;
        }
    }
}

static void read_multiline_input(const char* prompt, char* buffer, size_t buffer_size) {
    printf("%s: ", prompt);
    
    if (fgets(buffer, buffer_size, stdin)) {
        // Remove newline
        buffer[strcspn(buffer, "\n")] = 0;
    }
}

// --- Directory Helper Functions ---
static bool create_directory(const char* path) {
    struct stat st = {0};
    
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) == 0) {
            printf("[CPM Init] Created directory: %s\n", path);
            return true;
        } else {
            printf("[CPM Init] Failed to create directory: %s\n", path);
            return false;
        }
    }
    
    return true; // Directory already exists
}

// --- Package Spec Generation ---
static bool write_package_spec(const char* name, const char* version, const char* description,
                              const char* author, const char* license, const char* homepage,
                              const char* repository) {
    FILE* f = fopen("cpm_package.spec", "w");
    if (!f) {
        printf("[CPM Init] Error: Cannot create cpm_package.spec\n");
        return false;
    }
    
    fprintf(f, "{\n");
    fprintf(f, "  \"name\": \"%s\",\n", name);
    fprintf(f, "  \"version\": \"%s\",\n", version);
    fprintf(f, "  \"description\": \"%s\",\n", description);
    fprintf(f, "  \"author\": \"%s\",\n", author);
    fprintf(f, "  \"license\": \"%s\",\n", license);
    
    if (homepage && strlen(homepage) > 0) {
        fprintf(f, "  \"homepage\": \"%s\",\n", homepage);
    }
    
    if (repository && strlen(repository) > 0) {
        fprintf(f, "  \"repository\": \"%s\",\n", repository);
    }
    
    fprintf(f, "  \"main\": \"src/%s.c\",\n", name);
    fprintf(f, "  \"include_dir\": \"include\",\n");
    fprintf(f, "  \"lib_dir\": \"lib\",\n");
    fprintf(f, "  \"build_type\": \"library\",\n");
    fprintf(f, "  \"c_standard\": \"c11\",\n");
    fprintf(f, "  \"compiler_flags\": [\"-Wall\", \"-Wextra\", \"-O2\"],\n");
    fprintf(f, "  \"dependencies\": {},\n");
    fprintf(f, "  \"dev_dependencies\": {},\n");
    fprintf(f, "  \"scripts\": {\n");
    fprintf(f, "    \"build\": \"make\",\n");
    fprintf(f, "    \"test\": \"make test\",\n");
    fprintf(f, "    \"clean\": \"make clean\"\n");
    fprintf(f, "  }\n");
    fprintf(f, "}\n");
    
    fclose(f);
    printf("[CPM Init] Created cpm_package.spec\n");
    return true;
}

// --- CMakeLists.txt Generation ---
static bool write_cmake_file(const char* name, const char* version) {
    FILE* f = fopen("CMakeLists.txt", "w");
    if (!f) {
        printf("[CPM Init] Warning: Cannot create CMakeLists.txt\n");
        return false;
    }
    
    fprintf(f, "cmake_minimum_required(VERSION 3.10)\n");
    fprintf(f, "project(%s VERSION %s LANGUAGES C)\n\n", name, version);
    
    fprintf(f, "# Set C standard\n");
    fprintf(f, "set(CMAKE_C_STANDARD 11)\n");
    fprintf(f, "set(CMAKE_C_STANDARD_REQUIRED ON)\n\n");
    
    fprintf(f, "# Compiler flags\n");
    fprintf(f, "set(CMAKE_C_FLAGS \"${CMAKE_C_FLAGS} -Wall -Wextra\")\n");
    fprintf(f, "set(CMAKE_C_FLAGS_DEBUG \"-g -O0\")\n");
    fprintf(f, "set(CMAKE_C_FLAGS_RELEASE \"-O2 -DNDEBUG\")\n\n");
    
    fprintf(f, "# Include directories\n");
    fprintf(f, "include_directories(include)\n\n");
    
    fprintf(f, "# Source files\n");
    fprintf(f, "file(GLOB_RECURSE SOURCES \"src/*.c\")\n");
    fprintf(f, "file(GLOB_RECURSE HEADERS \"include/*.h\")\n\n");
    
    fprintf(f, "# Create library\n");
    fprintf(f, "add_library(%s ${SOURCES} ${HEADERS})\n\n", name);
    
    fprintf(f, "# Create example executable (optional)\n");
    fprintf(f, "if(EXISTS \"${CMAKE_CURRENT_SOURCE_DIR}/examples/main.c\")\n");
    fprintf(f, "    add_executable(%s_example examples/main.c)\n", name);
    fprintf(f, "    target_link_libraries(%s_example %s)\n", name, name);
    fprintf(f, "endif()\n\n");
    
    fprintf(f, "# Install targets\n");
    fprintf(f, "install(TARGETS %s\n", name);
    fprintf(f, "    LIBRARY DESTINATION lib\n");
    fprintf(f, "    ARCHIVE DESTINATION lib)\n");
    fprintf(f, "install(DIRECTORY include/ DESTINATION include)\n");
    
    fclose(f);
    printf("[CPM Init] Created CMakeLists.txt\n");
    return true;
}

// --- Makefile Generation ---
static bool write_makefile(const char* name) {
    FILE* f = fopen("Makefile", "w");
    if (!f) {
        printf("[CPM Init] Warning: Cannot create Makefile\n");
        return false;
    }
    
    fprintf(f, "# Makefile for %s\n", name);
    fprintf(f, "# Generated by CPM init\n\n");
    
    fprintf(f, "CC = gcc\n");
    fprintf(f, "CFLAGS = -Wall -Wextra -std=c11 -Iinclude\n");
    fprintf(f, "CFLAGS_DEBUG = -g -O0 -DDEBUG\n");
    fprintf(f, "CFLAGS_RELEASE = -O2 -DNDEBUG\n\n");
    
    fprintf(f, "SRCDIR = src\n");
    fprintf(f, "INCDIR = include\n");
    fprintf(f, "BUILDDIR = build\n");
    fprintf(f, "LIBDIR = lib\n\n");
    
    fprintf(f, "SOURCES = $(wildcard $(SRCDIR)/*.c)\n");
    fprintf(f, "OBJECTS = $(SOURCES:$(SRCDIR)/%%.c=$(BUILDDIR)/%%.o)\n");
    fprintf(f, "LIBRARY = $(LIBDIR)/lib%s.a\n\n", name);
    
    fprintf(f, ".PHONY: all clean debug release test\n\n");
    
    fprintf(f, "all: $(LIBRARY)\n\n");
    
    fprintf(f, "debug: CFLAGS += $(CFLAGS_DEBUG)\n");
    fprintf(f, "debug: $(LIBRARY)\n\n");
    
    fprintf(f, "release: CFLAGS += $(CFLAGS_RELEASE)\n");
    fprintf(f, "release: $(LIBRARY)\n\n");
    
    fprintf(f, "$(LIBRARY): $(OBJECTS) | $(LIBDIR)\n");
    fprintf(f, "\tar rcs $@ $^\n\n");
    
    fprintf(f, "$(BUILDDIR)/%%.o: $(SRCDIR)/%%.c | $(BUILDDIR)\n");
    fprintf(f, "\t$(CC) $(CFLAGS) -c $< -o $@\n\n");
    
    fprintf(f, "$(BUILDDIR):\n");
    fprintf(f, "\tmkdir -p $(BUILDDIR)\n\n");
    
    fprintf(f, "$(LIBDIR):\n");
    fprintf(f, "\tmkdir -p $(LIBDIR)\n\n");
    
    fprintf(f, "test: $(LIBRARY)\n");
    fprintf(f, "\t@echo \"Running tests...\"\n");
    fprintf(f, "\t# Add test commands here\n\n");
    
    fprintf(f, "clean:\n");
    fprintf(f, "\trm -rf $(BUILDDIR) $(LIBDIR)\n\n");
    
    fprintf(f, "install: $(LIBRARY)\n");
    fprintf(f, "\tcp $(LIBRARY) /usr/local/lib/\n");
    fprintf(f, "\tcp -r $(INCDIR)/* /usr/local/include/\n");
    
    fclose(f);
    printf("[CPM Init] Created Makefile\n");
    return true;
}

// --- Create Source Files ---
static bool create_source_files(const char* name) {
    char filename[256];
    
    // Create main source file
    snprintf(filename, sizeof(filename), "src/%s.c", name);
    FILE* f = fopen(filename, "w");
    if (!f) {
        printf("[CPM Init] Warning: Cannot create %s\n", filename);
        return false;
    }
    
    fprintf(f, "/*\n");
    fprintf(f, " * File: %s.c\n", name);
    fprintf(f, " * Description: Main implementation for %s library\n", name);
    fprintf(f, " * Generated by CPM init\n");
    fprintf(f, " */\n\n");
    
    fprintf(f, "#include \"%s.h\"\n", name);
    fprintf(f, "#include <stdio.h>\n");
    fprintf(f, "#include <stdlib.h>\n\n");
    
    fprintf(f, "void %s_hello(void) {\n", name);
    fprintf(f, "    printf(\"Hello from %s!\\n\");\n", name);
    fprintf(f, "}\n\n");
    
    fprintf(f, "int %s_version_major(void) {\n", name);
    fprintf(f, "    return 1;\n");
    fprintf(f, "}\n\n");
    
    fprintf(f, "int %s_version_minor(void) {\n", name);
    fprintf(f, "    return 0;\n");
    fprintf(f, "}\n");
    
    fclose(f);
    printf("[CPM Init] Created %s\n", filename);
    
    // Create header file
    snprintf(filename, sizeof(filename), "include/%s.h", name);
    f = fopen(filename, "w");
    if (!f) {
        printf("[CPM Init] Warning: Cannot create %s\n", filename);
        return false;
    }
    
    // Convert name to uppercase for header guard
    char guard_name[256];
    snprintf(guard_name, sizeof(guard_name), "%s_H", name);
    for (int i = 0; guard_name[i]; i++) {
        if (guard_name[i] >= 'a' && guard_name[i] <= 'z') {
            guard_name[i] = guard_name[i] - 'a' + 'A';
        }
    }
    
    fprintf(f, "/*\n");
    fprintf(f, " * File: %s.h\n", name);
    fprintf(f, " * Description: Header file for %s library\n", name);
    fprintf(f, " * Generated by CPM init\n");
    fprintf(f, " */\n\n");
    
    fprintf(f, "#ifndef %s\n", guard_name);
    fprintf(f, "#define %s\n\n", guard_name);
    
    fprintf(f, "#ifdef __cplusplus\n");
    fprintf(f, "extern \"C\" {\n");
    fprintf(f, "#endif\n\n");
    
    fprintf(f, "/**\n");
    fprintf(f, " * Print a hello message from the %s library\n", name);
    fprintf(f, " */\n");
    fprintf(f, "void %s_hello(void);\n\n", name);
    
    fprintf(f, "/**\n");
    fprintf(f, " * Get the major version number\n");
    fprintf(f, " * @return Major version number\n");
    fprintf(f, " */\n");
    fprintf(f, "int %s_version_major(void);\n\n", name);
    
    fprintf(f, "/**\n");
    fprintf(f, " * Get the minor version number\n");
    fprintf(f, " * @return Minor version number\n");
    fprintf(f, " */\n");
    fprintf(f, "int %s_version_minor(void);\n\n", name);
    
    fprintf(f, "#ifdef __cplusplus\n");
    fprintf(f, "}\n");
    fprintf(f, "#endif\n\n");
    
    fprintf(f, "#endif /* %s */\n", guard_name);
    
    fclose(f);
    printf("[CPM Init] Created %s\n", filename);
    
    return true;
}

// --- Create Example File ---
static bool create_example_file(const char* name) {
    FILE* f = fopen("examples/main.c", "w");
    if (!f) {
        printf("[CPM Init] Warning: Cannot create examples/main.c\n");
        return false;
    }
    
    fprintf(f, "/*\n");
    fprintf(f, " * File: main.c\n");
    fprintf(f, " * Description: Example usage of %s library\n", name);
    fprintf(f, " * Generated by CPM init\n");
    fprintf(f, " */\n\n");
    
    fprintf(f, "#include \"%s.h\"\n", name);
    fprintf(f, "#include <stdio.h>\n\n");
    
    fprintf(f, "int main(void) {\n");
    fprintf(f, "    printf(\"Example program for %s\\n\");\n", name);
    fprintf(f, "    printf(\"Version: %%d.%%d\\n\", %s_version_major(), %s_version_minor());\n", name, name);
    fprintf(f, "    \n");
    fprintf(f, "    %s_hello();\n", name);
    fprintf(f, "    \n");
    fprintf(f, "    return 0;\n");
    fprintf(f, "}\n");
    
    fclose(f);
    printf("[CPM Init] Created examples/main.c\n");
    return true;
}

// --- Main Init Command Handler ---
CPM_Result cpm_handle_init_command(int argc, char* argv[], const CPM_Config* config) {
    (void)config; // Suppress unused parameter warning
    
    printf("[CPM Init] Initializing new C package\n");
    
    // Check if already initialized
    if (access("cpm_package.spec", F_OK) == 0) {
        char response[10];
        printf("[CPM Init] cpm_package.spec already exists. Overwrite? (y/N): ");
        if (fgets(response, sizeof(response), stdin)) {
            if (response[0] != 'y' && response[0] != 'Y') {
                printf("[CPM Init] Initialization cancelled\n");
                return CPM_RESULT_SUCCESS;
            }
        }
    }
    
    // Get current directory name as default package name
    char* cwd = getcwd(NULL, 0);
    char* default_name = strrchr(cwd, '/');
    if (default_name) {
        default_name++; // Skip the '/'
    } else {
        default_name = "my-package";
    }
    
    // Interactive package information gathering
    char name[256], version[64], description[512];
    char author[256], license[64], homepage[512], repository[512];
    
    printf("\nThis utility will walk you through creating a cpm_package.spec file.\n");
    printf("Press ^C at any time to quit.\n\n");
    
    // Gather package information
    read_input("Package name", default_name, name, sizeof(name));
    read_input("Version", "1.0.0", version, sizeof(version));
    read_multiline_input("Description", description, sizeof(description));
    read_input("Author", "", author, sizeof(author));
    read_input("License", "MIT", license, sizeof(license));
    read_input("Homepage", "", homepage, sizeof(homepage));
    read_input("Repository", "", repository, sizeof(repository));
    
    free(cwd);
    
    printf("\n[CPM Init] Creating package structure...\n");
    
    // Create directory structure
    create_directory("src");
    create_directory("include");
    create_directory("lib");
    create_directory("examples");
    create_directory("tests");
    create_directory("docs");
    
    // Create package specification
    if (!write_package_spec(name, version, description, author, license, homepage, repository)) {
        return CPM_RESULT_ERROR_COMMAND_FAILED;
    }
    
    // Create build files
    write_makefile(name);
    write_cmake_file(name, version);
    
    // Create source files
    create_source_files(name);
    create_example_file(name);
    
    // Create README.md
    FILE* readme = fopen("README.md", "w");
    if (readme) {
        fprintf(readme, "# %s\n\n", name);
        fprintf(readme, "%s\n\n", description);
        fprintf(readme, "## Installation\n\n");
        fprintf(readme, "```bash\n");
        fprintf(readme, "cpm install %s\n", name);
        fprintf(readme, "```\n\n");
        fprintf(readme, "## Usage\n\n");
        fprintf(readme, "```c\n");
        fprintf(readme, "#include \"%s.h\"\n\n", name);
        fprintf(readme, "int main(void) {\n");
        fprintf(readme, "    %s_hello();\n", name);
        fprintf(readme, "    return 0;\n");
        fprintf(readme, "}\n");
        fprintf(readme, "```\n\n");
        fprintf(readme, "## Building\n\n");
        fprintf(readme, "```bash\n");
        fprintf(readme, "make\n");
        fprintf(readme, "```\n\n");
        fprintf(readme, "## License\n\n");
        fprintf(readme, "%s\n", license);
        fclose(readme);
        printf("[CPM Init] Created README.md\n");
    }
    
    // Create .gitignore
    FILE* gitignore = fopen(".gitignore", "w");
    if (gitignore) {
        fprintf(gitignore, "# Build artifacts\n");
        fprintf(gitignore, "build/\n");
        fprintf(gitignore, "lib/\n");
        fprintf(gitignore, "*.o\n");
        fprintf(gitignore, "*.a\n");
        fprintf(gitignore, "*.so\n");
        fprintf(gitignore, "*.dylib\n\n");
        fprintf(gitignore, "# CPM modules\n");
        fprintf(gitignore, "cpm_modules/\n\n");
        fprintf(gitignore, "# IDE files\n");
        fprintf(gitignore, ".vscode/\n");
        fprintf(gitignore, ".idea/\n");
        fprintf(gitignore, "*.swp\n");
        fprintf(gitignore, "*.swo\n\n");
        fprintf(gitignore, "# OS files\n");
        fprintf(gitignore, ".DS_Store\n");
        fprintf(gitignore, "Thumbs.db\n");
        fclose(gitignore);
        printf("[CPM Init] Created .gitignore\n");
    }
    
    printf("\n[CPM Init] Package initialization complete!\n");
    printf("[CPM Init] Package: %s@%s\n", name, version);
    printf("[CPM Init] \n");
    printf("[CPM Init] Next steps:\n");
    printf("[CPM Init]   1. Edit src/%s.c and include/%s.h to implement your library\n", name, name);
    printf("[CPM Init]   2. Run 'make' to build your library\n");
    printf("[CPM Init]   3. Run 'make test' to run tests\n");
    printf("[CPM Init]   4. Run 'cpm publish' to publish to the registry\n");
    
    return CPM_RESULT_SUCCESS;
}
