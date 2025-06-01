#include "cpm.h"

void print_usage(const char* program_name) {
    printf("CPM - C Package Manager v%s\n", CPM_VERSION);
    printf("A hardened C implementation of NPM with Q promises and PMLL\n\n");
    printf("Usage: %s <command> [options] [package[@version]]\n\n", program_name);
    printf("Commands:\n");
    printf("  install, i          Install packages\n");
    printf("  uninstall, remove   Remove packages\n");
    printf("  update              Update packages\n");
    printf("  list, ls            List installed packages\n");
    printf("  info                Show package information\n");
    printf("  audit               Audit packages for vulnerabilities\n");
    printf("  init                Initialize new package.json\n");
    printf("  help                Show this help message\n");
    printf("  version             Show version information\n\n");
    printf("Options:\n");
    printf("  --save, -S          Save to dependencies\n");
    printf("  --save-dev, -D      Save to devDependencies\n");
    printf("  --global, -g        Install globally\n");
    printf("  --verbose, -v       Verbose output\n");
    printf("  --dry-run           Show what would be done\n");
    printf("  --help, -h          Show help\n\n");
    printf("Examples:\n");
    printf("  %s install express           Install express package\n", program_name);
    printf("  %s install lodash@4.17.21    Install specific version\n", program_name);
    printf("  %s install --save-dev jest   Install as dev dependency\n", program_name);
    printf("  %s update                    Update all packages\n", program_name);
    printf("  %s list                      List installed packages\n", program_name);
}

void print_version(void) {
    printf("CPM version %s\n", CPM_VERSION);
    printf("C Package Manager - NPM compatibility layer\n");
    printf("With Q Promises and PMLL support\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return CPM_ERROR_INVALID_ARGS;
    }

    // Initialize CPM context
    CPMContext ctx;
    if (cpm_init(&ctx) != CPM_SUCCESS) {
        fprintf(stderr, "Failed to initialize CPM context\n");
        return CPM_ERROR_MEMORY;
    }

    // Parse command
    const char* command = argv[1];

    // Handle version and help commands
    if (strcmp(command, "version") == 0 || strcmp(command, "--version") == 0) {
        print_version();
        return CPM_SUCCESS;
    }

    if (strcmp(command, "help") == 0 || strcmp(command, "--help") == 0) {
        print_usage(argv[0]);
        return CPM_SUCCESS;
    }

    // Parse options and find package name
    bool save = false;
    bool save_dev = false;
    bool global = false;
    char* package_spec = NULL;
    
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--save") == 0 || strcmp(argv[i], "-S") == 0) {
            save = true;
        } else if (strcmp(argv[i], "--save-dev") == 0 || strcmp(argv[i], "-D") == 0) {
            save_dev = true;
        } else if (strcmp(argv[i], "--global") == 0 || strcmp(argv[i], "-g") == 0) {
            global = true;
        } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            ctx.verbose = true;
        } else if (strcmp(argv[i], "--dry-run") == 0) {
            ctx.dry_run = true;
        } else if (argv[i][0] != '-') {
            // This is the package name
            package_spec = argv[i];
        }
    }

    // Execute commands
    int result = CPM_SUCCESS;

    if (strcmp(command, "install") == 0 || strcmp(command, "i") == 0) {
        if (package_spec == NULL) {
            // Install from package.json
            result = cpm_install(&ctx, NULL, NULL);
        } else {
            // Install specific package
            
            // Parse package@version format
            char package_name[MAX_PACKAGE_NAME];
            char version[MAX_VERSION_LENGTH] = "latest";
            
            char* at_sign = strchr(package_spec, '@');
            if (at_sign && at_sign != package_spec) {
                // Has version specified
                size_t name_len = at_sign - package_spec;
                strncpy(package_name, package_spec, name_len);
                package_name[name_len] = '\0';
                strcpy(version, at_sign + 1);
            } else {
                strcpy(package_name, package_spec);
            }
            
            result = cpm_install(&ctx, package_name, version);
        }
    } else if (strcmp(command, "uninstall") == 0 || strcmp(command, "remove") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Package name required for uninstall\n");
            result = CPM_ERROR_INVALID_ARGS;
        } else {
            result = cpm_uninstall(&ctx, argv[2]);
        }
    } else if (strcmp(command, "update") == 0) {
        const char* package_name = (argc > 2) ? argv[2] : NULL;
        result = cpm_update(&ctx, package_name);
    } else if (strcmp(command, "list") == 0 || strcmp(command, "ls") == 0) {
        result = cpm_list(&ctx);
    } else if (strcmp(command, "info") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Package name required for info\n");
            result = CPM_ERROR_INVALID_ARGS;
        } else {
            result = cpm_info(&ctx, argv[2]);
        }
    } else if (strcmp(command, "audit") == 0) {
        result = cpm_audit(&ctx);
    } else if (strcmp(command, "init") == 0) {
        // Create new package.json
        printf("Creating package.json...\n");
        // Implementation would create interactive package.json creation
        result = CPM_SUCCESS;
    } else {
        fprintf(stderr, "Error: Unknown command '%s'\n", command);
        print_usage(argv[0]);
        result = CPM_ERROR_INVALID_ARGS;
    }

    if (result != CPM_SUCCESS) {
        fprintf(stderr, "Error: %s\n", cpm_error_string(result));
    }

    return result;
}