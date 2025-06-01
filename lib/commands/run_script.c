/*
 * File: run_script.c
 * Description: CPM run-script command implementation - npm run equivalent for C packages
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

// --- Script Execution ---
static bool execute_script(const char* script_command, const char* script_name) {
    printf("[CPM Run-Script] Executing '%s' script: %s\n", script_name, script_command);
    
    int result = system(script_command);
    
    if (result == 0) {
        printf("[CPM Run-Script] Script '%s' completed successfully\n", script_name);
        return true;
    } else {
        printf("[CPM Run-Script] Script '%s' failed with exit code: %d\n", script_name, result);
        return false;
    }
}

// --- Parse Scripts from Package Spec ---
static char* find_script_in_spec(const char* script_name, const char* spec_content) {
    // Simple JSON-like parsing for scripts section
    // In a real implementation, you'd use a proper JSON parser
    
    char* scripts_start = strstr(spec_content, "\"scripts\":");
    if (!scripts_start) {
        return NULL;
    }
    
    // Find the opening brace for scripts object
    char* brace_start = strchr(scripts_start, '{');
    if (!brace_start) {
        return NULL;
    }
    
    // Find the closing brace for scripts object
    char* brace_end = strchr(brace_start + 1, '}');
    if (!brace_end) {
        return NULL;
    }
    
    // Search for the script name within the scripts object
    char search_pattern[256];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", script_name);
    
    char* script_line = strstr(brace_start, search_pattern);
    if (!script_line || script_line > brace_end) {
        return NULL;
    }
    
    // Find the start of the script command (after the colon and quote)
    char* command_start = strchr(script_line + strlen(search_pattern), '"');
    if (!command_start) {
        return NULL;
    }
    command_start++; // Skip the opening quote
    
    // Find the end of the script command
    char* command_end = strchr(command_start, '"');
    if (!command_end) {
        return NULL;
    }
    
    // Extract the command
    size_t command_length = command_end - command_start;
    char* command = malloc(command_length + 1);
    if (!command) {
        return NULL;
    }
    
    strncpy(command, command_start, command_length);
    command[command_length] = '\0';
    
    return command;
}

// --- Load and Parse Package Spec ---
static char* load_package_spec(void) {
    FILE* f = fopen("cpm_package.spec", "r");
    if (!f) {
        printf("[CPM Run-Script] Error: cpm_package.spec not found in current directory\n");
        return NULL;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size <= 0) {
        printf("[CPM Run-Script] Error: cpm_package.spec is empty\n");
        fclose(f);
        return NULL;
    }
    
    // Read file content
    char* content = malloc(file_size + 1);
    if (!content) {
        printf("[CPM Run-Script] Error: Memory allocation failed\n");
        fclose(f);
        return NULL;
    }
    
    size_t bytes_read = fread(content, 1, file_size, f);
    content[bytes_read] = '\0';
    
    fclose(f);
    return content;
}

// --- List Available Scripts ---
static void list_available_scripts(const char* spec_content) {
    printf("[CPM Run-Script] Available scripts:\n");
    
    char* scripts_start = strstr(spec_content, "\"scripts\":");
    if (!scripts_start) {
        printf("  No scripts defined in cpm_package.spec\n");
        return;
    }
    
    // Find the opening brace for scripts object
    char* brace_start = strchr(scripts_start, '{');
    if (!brace_start) {
        printf("  Invalid scripts section in cpm_package.spec\n");
        return;
    }
    
    // Find the closing brace for scripts object
    char* brace_end = strchr(brace_start + 1, '}');
    if (!brace_end) {
        printf("  Invalid scripts section in cpm_package.spec\n");
        return;
    }
    
    // Parse script entries (simple approach)
    char* current = brace_start + 1;
    bool found_any = false;
    
    while (current < brace_end) {
        // Find next quote
        char* quote_start = strchr(current, '"');
        if (!quote_start || quote_start >= brace_end) {
            break;
        }
        
        // Find end of script name
        char* quote_end = strchr(quote_start + 1, '"');
        if (!quote_end || quote_end >= brace_end) {
            break;
        }
        
        // Extract script name
        size_t name_length = quote_end - quote_start - 1;
        char script_name[256];
        if (name_length < sizeof(script_name)) {
            strncpy(script_name, quote_start + 1, name_length);
            script_name[name_length] = '\0';
            
            // Find the command
            char* colon = strchr(quote_end, ':');
            if (colon && colon < brace_end) {
                char* cmd_quote_start = strchr(colon, '"');
                if (cmd_quote_start && cmd_quote_start < brace_end) {
                    char* cmd_quote_end = strchr(cmd_quote_start + 1, '"');
                    if (cmd_quote_end && cmd_quote_end < brace_end) {
                        size_t cmd_length = cmd_quote_end - cmd_quote_start - 1;
                        char command[512];
                        if (cmd_length < sizeof(command)) {
                            strncpy(command, cmd_quote_start + 1, cmd_length);
                            command[cmd_length] = '\0';
                            
                            printf("  %-15s %s\n", script_name, command);
                            found_any = true;
                        }
                    }
                }
            }
        }
        
        current = quote_end + 1;
    }
    
    if (!found_any) {
        printf("  No valid scripts found in cpm_package.spec\n");
    }
    
    printf("\nUsage: cpm run-script <script-name>\n");
}

// --- Main Run-Script Command Handler ---
CPM_Result cpm_handle_run_script_command(int argc, char* argv[], const CPM_Config* config) {
    (void)config; // Suppress unused parameter warning
    
    // Load package specification
    char* spec_content = load_package_spec();
    if (!spec_content) {
        return CPM_RESULT_ERROR_COMMAND_FAILED;
    }
    
    // If no script name provided, list available scripts
    if (argc == 0 || !argv[0] || strlen(argv[0]) == 0) {
        list_available_scripts(spec_content);
        free(spec_content);
        return CPM_RESULT_SUCCESS;
    }
    
    const char* script_name = argv[0];
    printf("[CPM Run-Script] Looking for script: %s\n", script_name);
    
    // Find the script in the package spec
    char* script_command = find_script_in_spec(script_name, spec_content);
    
    if (!script_command) {
        printf("[CPM Run-Script] Error: Script '%s' not found in cpm_package.spec\n", script_name);
        printf("[CPM Run-Script] \n");
        list_available_scripts(spec_content);
        free(spec_content);
        return CPM_RESULT_ERROR_COMMAND_FAILED;
    }
    
    // Execute the script
    bool success = execute_script(script_command, script_name);
    
    // Cleanup
    free(script_command);
    free(spec_content);
    
    return success ? CPM_RESULT_SUCCESS : CPM_RESULT_ERROR_COMMAND_FAILED;
}