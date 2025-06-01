/*
 * File: cpm_semver.c
 * Description: Semantic versioning implementation for CPM
 * Author: Dr. Q Josef Kurk Edwards
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "cpm_semver.h"

// --- Internal Helper Functions ---
static char* strdup_safe(const char* str) {
    if (!str) return NULL;
    return strdup(str);
}

static bool is_digit_string(const char* str) {
    if (!str || *str == '\0') return false;
    for (const char* p = str; *p; p++) {
        if (!isdigit(*p)) return false;
    }
    return true;
}

// --- Version Parsing ---
SemVer* semver_parse(const char* version_string) {
    if (!version_string) return NULL;
    
    SemVer* version = calloc(1, sizeof(SemVer));
    if (!version) return NULL;
    
    // Make a copy to work with
    char* work_string = strdup(version_string);
    if (!work_string) {
        free(version);
        return NULL;
    }
    
    // Remove leading 'v' if present
    char* start = work_string;
    if (*start == 'v' || *start == 'V') {
        start++;
    }
    
    // Split on '+' to separate build metadata
    char* build_part = strchr(start, '+');
    if (build_part) {
        *build_part = '\0';
        build_part++;
        version->build = strdup_safe(build_part);
    }
    
    // Split on '-' to separate prerelease
    char* prerelease_part = strchr(start, '-');
    if (prerelease_part) {
        *prerelease_part = '\0';
        prerelease_part++;
        version->prerelease = strdup_safe(prerelease_part);
    }
    
    // Parse major.minor.patch
    char* major_str = strtok(start, ".");
    char* minor_str = strtok(NULL, ".");
    char* patch_str = strtok(NULL, ".");
    
    if (!major_str || !is_digit_string(major_str)) {
        goto parse_error;
    }
    version->major = atoi(major_str);
    
    if (!minor_str || !is_digit_string(minor_str)) {
        goto parse_error;
    }
    version->minor = atoi(minor_str);
    
    if (!patch_str || !is_digit_string(patch_str)) {
        goto parse_error;
    }
    version->patch = atoi(patch_str);
    
    free(work_string);
    return version;
    
parse_error:
    semver_free(version);
    free(work_string);
    return NULL;
}

void semver_free(SemVer* version) {
    if (!version) return;
    
    free(version->prerelease);
    free(version->build);
    free(version);
}

char* semver_to_string(const SemVer* version) {
    if (!version) return NULL;
    
    char* result = malloc(256);
    if (!result) return NULL;
    
    int len = snprintf(result, 256, "%d.%d.%d", version->major, version->minor, version->patch);
    
    if (version->prerelease) {
        len += snprintf(result + len, 256 - len, "-%s", version->prerelease);
    }
    
    if (version->build) {
        len += snprintf(result + len, 256 - len, "+%s", version->build);
    }
    
    return result;
}

// --- Version Comparison ---
int semver_compare(const SemVer* a, const SemVer* b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    
    // Compare major.minor.patch
    if (a->major != b->major) return (a->major > b->major) ? 1 : -1;
    if (a->minor != b->minor) return (a->minor > b->minor) ? 1 : -1;
    if (a->patch != b->patch) return (a->patch > b->patch) ? 1 : -1;
    
    // Compare prerelease (version without prerelease > version with prerelease)
    if (!a->prerelease && !b->prerelease) return 0;
    if (!a->prerelease && b->prerelease) return 1;
    if (a->prerelease && !b->prerelease) return -1;
    
    // Both have prerelease - compare lexically
    return strcmp(a->prerelease, b->prerelease);
}

bool semver_equals(const SemVer* a, const SemVer* b) {
    return semver_compare(a, b) == 0;
}

bool semver_greater(const SemVer* a, const SemVer* b) {
    return semver_compare(a, b) > 0;
}

bool semver_less(const SemVer* a, const SemVer* b) {
    return semver_compare(a, b) < 0;
}

// --- Constraint Parsing ---
VersionConstraint* semver_parse_constraint(const char* constraint_string) {
    if (!constraint_string) return NULL;
    
    VersionConstraint* constraint = calloc(1, sizeof(VersionConstraint));
    if (!constraint) return NULL;
    
    char* work_string = strdup(constraint_string);
    if (!work_string) {
        free(constraint);
        return NULL;
    }
    
    // Trim whitespace
    char* start = work_string;
    while (isspace(*start)) start++;
    
    char* end = start + strlen(start) - 1;
    while (end > start && isspace(*end)) *end-- = '\0';
    
    // Parse constraint type and version
    if (strcmp(start, "*") == 0 || strcmp(start, "latest") == 0) {
        constraint->type = CONSTRAINT_ANY;
    } else if (strstr(start, " - ") != NULL) {
        // Range constraint: "1.0.0 - 2.0.0"
        constraint->type = CONSTRAINT_RANGE;
        char* dash = strstr(start, " - ");
        *dash = '\0';
        constraint->version = semver_parse(start);
        constraint->version_max = semver_parse(dash + 3);
        if (!constraint->version || !constraint->version_max) {
            goto constraint_error;
        }
    } else if (start[0] == '^') {
        // Compatible constraint: "^1.2.3"
        constraint->type = CONSTRAINT_COMPATIBLE;
        constraint->version = semver_parse(start + 1);
    } else if (start[0] == '~') {
        // Tilde constraint: "~1.2.3"
        constraint->type = CONSTRAINT_TILDE;
        constraint->version = semver_parse(start + 1);
    } else if (strncmp(start, ">=", 2) == 0) {
        // Greater than or equal: ">=1.2.3"
        constraint->type = CONSTRAINT_GREATER_EQ;
        constraint->version = semver_parse(start + 2);
    } else if (strncmp(start, "<=", 2) == 0) {
        // Less than or equal: "<=1.2.3"
        constraint->type = CONSTRAINT_LESS_EQ;
        constraint->version = semver_parse(start + 2);
    } else if (start[0] == '>') {
        // Greater than: ">1.2.3"
        constraint->type = CONSTRAINT_GREATER;
        constraint->version = semver_parse(start + 1);
    } else if (start[0] == '<') {
        // Less than: "<1.2.3"
        constraint->type = CONSTRAINT_LESS;
        constraint->version = semver_parse(start + 1);
    } else {
        // Exact constraint: "1.2.3"
        constraint->type = CONSTRAINT_EXACT;
        constraint->version = semver_parse(start);
    }
    
    free(work_string);
    
    if (constraint->type != CONSTRAINT_ANY && !constraint->version) {
        goto constraint_error;
    }
    
    return constraint;
    
constraint_error:
    semver_free_constraint(constraint);
    free(work_string);
    return NULL;
}

void semver_free_constraint(VersionConstraint* constraint) {
    if (!constraint) return;
    
    semver_free(constraint->version);
    semver_free(constraint->version_max);
    free(constraint);
}

bool semver_satisfies(const SemVer* version, const VersionConstraint* constraint) {
    if (!version || !constraint) return false;
    
    switch (constraint->type) {
        case CONSTRAINT_ANY:
            return true;
            
        case CONSTRAINT_EXACT:
            return semver_equals(version, constraint->version);
            
        case CONSTRAINT_COMPATIBLE: {
            // ^1.2.3 := >=1.2.3 <2.0.0 (Same major version)
            if (version->major != constraint->version->major) return false;
            return semver_compare(version, constraint->version) >= 0;
        }
        
        case CONSTRAINT_TILDE: {
            // ~1.2.3 := >=1.2.3 <1.3.0 (Same major.minor version)
            if (version->major != constraint->version->major ||
                version->minor != constraint->version->minor) return false;
            return semver_compare(version, constraint->version) >= 0;
        }
        
        case CONSTRAINT_GREATER:
            return semver_greater(version, constraint->version);
            
        case CONSTRAINT_GREATER_EQ:
            return semver_compare(version, constraint->version) >= 0;
            
        case CONSTRAINT_LESS:
            return semver_less(version, constraint->version);
            
        case CONSTRAINT_LESS_EQ:
            return semver_compare(version, constraint->version) <= 0;
            
        case CONSTRAINT_RANGE:
            return semver_compare(version, constraint->version) >= 0 &&
                   semver_compare(version, constraint->version_max) <= 0;
    }
    
    return false;
}

// --- Version Utilities ---
bool semver_is_valid(const char* version_string) {
    SemVer* version = semver_parse(version_string);
    if (version) {
        semver_free(version);
        return true;
    }
    return false;
}

SemVer* semver_increment_major(const SemVer* version) {
    if (!version) return NULL;
    
    SemVer* new_version = calloc(1, sizeof(SemVer));
    if (!new_version) return NULL;
    
    new_version->major = version->major + 1;
    new_version->minor = 0;
    new_version->patch = 0;
    // Clear prerelease and build for major increment
    
    return new_version;
}

SemVer* semver_increment_minor(const SemVer* version) {
    if (!version) return NULL;
    
    SemVer* new_version = calloc(1, sizeof(SemVer));
    if (!new_version) return NULL;
    
    new_version->major = version->major;
    new_version->minor = version->minor + 1;
    new_version->patch = 0;
    // Clear prerelease and build for minor increment
    
    return new_version;
}

SemVer* semver_increment_patch(const SemVer* version) {
    if (!version) return NULL;
    
    SemVer* new_version = calloc(1, sizeof(SemVer));
    if (!new_version) return NULL;
    
    new_version->major = version->major;
    new_version->minor = version->minor;
    new_version->patch = version->patch + 1;
    // Clear prerelease and build for patch increment
    
    return new_version;
}

// --- Version Resolution ---
SemVer* semver_resolve_latest_compatible(const SemVer** versions, size_t count, const VersionConstraint* constraint) {
    if (!versions || count == 0 || !constraint) return NULL;
    
    SemVer* best = NULL;
    
    for (size_t i = 0; i < count; i++) {
        if (semver_satisfies(versions[i], constraint)) {
            if (!best || semver_greater(versions[i], best)) {
                best = (SemVer*)versions[i];
            }
        }
    }
    
    return best ? semver_parse(semver_to_string(best)) : NULL;
}

SemVer** semver_filter_versions(const SemVer** versions, size_t count, const VersionConstraint* constraint, size_t* result_count) {
    if (!versions || count == 0 || !constraint || !result_count) {
        if (result_count) *result_count = 0;
        return NULL;
    }
    
    SemVer** results = malloc(count * sizeof(SemVer*));
    if (!results) {
        *result_count = 0;
        return NULL;
    }
    
    size_t found = 0;
    for (size_t i = 0; i < count; i++) {
        if (semver_satisfies(versions[i], constraint)) {
            char* version_str = semver_to_string(versions[i]);
            results[found] = semver_parse(version_str);
            free(version_str);
            if (results[found]) found++;
        }
    }
    
    *result_count = found;
    return results;
}