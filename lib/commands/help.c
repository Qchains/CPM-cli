/*
 * File: help.c
 * Description: CPM help command implementation - displays usage information
 * Author: Dr. Q Josef Kurk Edwards
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpm.h"

// --- Command Help Information ---
typedef struct {
    const char* command;
    const char* usage;
    const char* description;
    const char* examples[3];
} CommandHelp;

static const CommandHelp command_help[] = {
    {
        .command = "install",
        .usage = "cpm install [package-name[@version]] [...]",
        .description = "Install one or more packages and their dependencies",
        .examples = {
            "cpm install libmath",
            "cpm install libmath@1.2.3",
            "cpm install libmath libutils libnetwork"
        }
    },
    {
        .command = "publish",
        .usage = "cpm publish [package-directory]",
        .description = "Publish a package to the CPM registry",
        .examples = {
            "cpm publish",
            "cpm publish ./my-package",
            NULL
        }
    },
    {
        .command = "search",
        .usage = "cpm search <query>",
        .description = "Search for packages in the CPM registry",
        .examples = {
            "cpm search math",
            "cpm search network library",
            NULL
        }
    },
    {
        .command = "init",
        .usage = "cpm init",
        .description = "Initialize a new C package in the current directory",
        .examples = {
            "cpm init",
            NULL,
            NULL
        }
    },
    {
        .command = "run-script",
        .usage = "cpm run-script <script-name>",
        .description = "Run a script defined in cpm_package.spec",
        .examples = {
            "cpm run-script build",
            "cpm run-script test",
            "cpm run-script clean"
        }
    },
    {
        .command = "help",
        .usage = "cpm help [command]",
        .description = "Display help information for CPM commands",
        .examples = {
            "cpm help",
            "cpm help install",
            "cpm help publish"
        }
    }
};

static const size_t num_commands = sizeof(command_help) / sizeof(command_help[0]);

// --- Display General Help ---
static void display_general_help(void) {
    printf("CPM - C Package Manager\n");
    printf("npm-like package manager for C libraries and applications\n\n");
    
    printf("Usage: cpm <command> [options]\n\n");
    
    printf("Commands:\n");
    for (size_t i = 0; i < num_commands; i++) {
        printf("  %-12s %s\n", command_help[i].command, command_help[i].description);
    }
    
    printf("\nGlobal Options:\n");
    printf("  -h, --help     Show help information\n");
    printf("  -v, --version  Show version information\n");
    printf("  --verbose      Enable verbose output\n");
    printf("  --quiet        Suppress non-error output\n");
    printf("  --registry     Specify alternate registry URL\n");
    
    printf("\nConfiguration:\n");
    printf("  CPM uses configuration files similar to npm:\n");
    printf("  - Global config: ~/.cpmrc\n");
    printf("  - Project config: ./.cpmrc\n");
    printf("  - Package spec: ./cpm_package.spec\n");
    
    printf("\nExamples:\n");
    printf("  cpm init                     # Initialize new package\n");
    printf("  cpm install libmath          # Install a package\n");
    printf("  cpm search networking        # Search for packages\n");
    printf("  cpm publish                  # Publish current package\n");
    printf("  cpm run-script build         # Run build script\n");
    
    printf("\nFor more information on a specific command, use:\n");
    printf("  cpm help <command>\n");
    
    printf("\nRegistry:\n");
    printf("  Default registry: http://localhost:8080\n");
    printf("  Set custom registry: cpm --registry=https://my-registry.com\n");
    
    printf("\nDocumentation: https://github.com/cpm/cpm\n");
    printf("Report bugs: https://github.com/cpm/cpm/issues\n");
}

// --- Display Command-Specific Help ---
static void display_command_help(const char* command_name) {
    for (size_t i = 0; i < num_commands; i++) {
        if (strcmp(command_help[i].command, command_name) == 0) {
            printf("CPM %s - %s\n\n", command_help[i].command, command_help[i].description);
            printf("Usage: %s\n\n", command_help[i].usage);
            
            printf("Description:\n");
            printf("  %s\n\n", command_help[i].description);
            
            // Display additional details based on command
            if (strcmp(command_name, "install") == 0) {
                printf("Options:\n");
                printf("  --save         Save to dependencies (default)\n");
                printf("  --save-dev     Save to dev dependencies\n");
                printf("  --global       Install globally\n");
                printf("  --force        Force reinstall\n");
                printf("  --no-deps      Skip dependency installation\n\n");
                
                printf("Package Specification:\n");
                printf("  package-name              # Latest version\n");
                printf("  package-name@1.2.3        # Specific version\n");
                printf("  package-name@^1.2.0       # Compatible version\n");
                printf("  package-name@~1.2.0       # Patch-level changes\n\n");
                
            } else if (strcmp(command_name, "publish") == 0) {
                printf("Requirements:\n");
                printf("  - Valid cpm_package.spec file\n");
                printf("  - Package name and version\n");
                printf("  - Valid registry authentication\n\n");
                
                printf("Process:\n");
                printf("  1. Validates package specification\n");
                printf("  2. Creates package archive (tar.gz)\n");
                printf("  3. Uploads to registry\n");
                printf("  4. Updates package index\n\n");
                
            } else if (strcmp(command_name, "search") == 0) {
                printf("Search Scope:\n");
                printf("  - Package names\n");
                printf("  - Package descriptions\n");
                printf("  - Package keywords\n");
                printf("  - Author names\n\n");
                
                printf("Output Format:\n");
                printf("  NAME         VERSION    DESCRIPTION              DOWNLOADS  AUTHOR\n");
                printf("  libmath      1.2.3      Mathematical library     1500       Math Team\n\n");
                
            } else if (strcmp(command_name, "init") == 0) {
                printf("Interactive Setup:\n");
                printf("  The init command will prompt for:\n");
                printf("  - Package name (default: current directory)\n");
                printf("  - Version (default: 1.0.0)\n");
                printf("  - Description\n");
                printf("  - Author\n");
                printf("  - License (default: MIT)\n");
                printf("  - Homepage URL\n");
                printf("  - Repository URL\n\n");
                
                printf("Generated Files:\n");
                printf("  - cpm_package.spec        # Package specification\n");
                printf("  - Makefile               # Build configuration\n");
                printf("  - CMakeLists.txt         # CMake configuration\n");
                printf("  - README.md              # Documentation\n");
                printf("  - .gitignore             # Git ignore file\n");
                printf("  - src/[name].c           # Main source file\n");
                printf("  - include/[name].h       # Main header file\n");
                printf("  - examples/main.c        # Example usage\n\n");
                
            } else if (strcmp(command_name, "run-script") == 0) {
                printf("Available Scripts:\n");
                printf("  Scripts are defined in the 'scripts' section of cpm_package.spec\n\n");
                
                printf("Common Scripts:\n");
                printf("  build      # Compile the package\n");
                printf("  test       # Run tests\n");
                printf("  clean      # Clean build artifacts\n");
                printf("  install    # Install the package\n");
                printf("  format     # Format source code\n\n");
                
                printf("Example cpm_package.spec scripts section:\n");
                printf("  \"scripts\": {\n");
                printf("    \"build\": \"make\",\n");
                printf("    \"test\": \"make test\",\n");
                printf("    \"clean\": \"make clean\",\n");
                printf("    \"format\": \"clang-format -i src/*.c include/*.h\"\n");
                printf("  }\n\n");
            }
            
            if (command_help[i].examples[0]) {
                printf("Examples:\n");
                for (int j = 0; j < 3 && command_help[i].examples[j]; j++) {
                    printf("  %s\n", command_help[i].examples[j]);
                }
                printf("\n");
            }
            
            return;
        }
    }
    
    printf("Unknown command: %s\n", command_name);
    printf("Use 'cpm help' to see available commands.\n");
}

// --- Display Version Information ---
static void display_version_info(void) {
    printf("CPM - C Package Manager\n");
    printf("Version: 1.0.0\n");
    printf("Build Date: %s %s\n", __DATE__, __TIME__);
    printf("Author: Dr. Q Josef Kurk Edwards\n");
    printf("License: MIT\n");
    printf("Homepage: https://github.com/cpm/cpm\n");
    
    printf("\nFeatures:\n");
    printf("  ✓ Package installation and management\n");
    printf("  ✓ Dependency resolution\n");
    printf("  ✓ Package publishing to registry\n");
    printf("  ✓ Package search and discovery\n");
    printf("  ✓ Project initialization\n");
    printf("  ✓ Script execution\n");
    printf("  ✓ Build system integration\n");
    printf("  ✓ Semantic versioning\n");
    printf("  ✓ Promise-based async operations\n");
    printf("  ✓ Thread-safe file operations\n");
    
    printf("\nSystem Information:\n");
    printf("  C Standard: C11\n");
    printf("  Dependencies: libcurl, pthread\n");
    printf("  Platform: POSIX-compatible\n");
}

// --- Main Help Command Handler ---
CPM_Result cpm_handle_help_command(int argc, char* argv[], const CPM_Config* config) {
    (void)config; // Suppress unused parameter warning
    
    if (argc == 0) {
        display_general_help();
        return CPM_RESULT_SUCCESS;
    }
    
    // Check for version flag
    if (strcmp(argv[0], "--version") == 0 || strcmp(argv[0], "-v") == 0) {
        display_version_info();
        return CPM_RESULT_SUCCESS;
    }
    
    // Display help for specific command
    display_command_help(argv[0]);
    return CPM_RESULT_SUCCESS;
}
