/*
 * File: cpm_semver.h
 * Description: Semantic versioning support for CPM - similar to npm semver
 * Author: Dr. Q Josef Kurk Edwards
 */

#ifndef CPM_SEMVER_H
#define CPM_SEMVER_H

#include <stdbool.h>
#include <stddef.h>

// --- Semantic Version Structure ---
typedef struct {
    int major;
    int minor;
    int patch;
    char* prerelease;    // e.g., "alpha", "beta.1"
    char* build;         // e.g., "20230101.sha1234"
} SemVer;

// --- Version Constraint Types ---
typedef enum {
    CONSTRAINT_EXACT,       // 1.2.3
    CONSTRAINT_COMPATIBLE,  // ^1.2.3 (compatible version)
    CONSTRAINT_TILDE,       // ~1.2.3 (patch-level changes)
    CONSTRAINT_GREATER,     // >1.2.3
    CONSTRAINT_GREATER_EQ,  // >=1.2.3
    CONSTRAINT_LESS,        // <1.2.3
    CONSTRAINT_LESS_EQ,     // <=1.2.3
    CONSTRAINT_RANGE,       // 1.2.3 - 2.0.0
    CONSTRAINT_ANY          // * or latest
} ConstraintType;

typedef struct {
    ConstraintType type;
    SemVer* version;
    SemVer* version_max; // For range constraints
} VersionConstraint;

// --- Version Parsing ---
SemVer* semver_parse(const char* version_string);
void semver_free(SemVer* version);
char* semver_to_string(const SemVer* version);

// --- Version Comparison ---
int semver_compare(const SemVer* a, const SemVer* b);
bool semver_equals(const SemVer* a, const SemVer* b);
bool semver_greater(const SemVer* a, const SemVer* b);
bool semver_less(const SemVer* a, const SemVer* b);

// --- Version Constraints ---
VersionConstraint* semver_parse_constraint(const char* constraint_string);
void semver_free_constraint(VersionConstraint* constraint);
bool semver_satisfies(const SemVer* version, const VersionConstraint* constraint);

// --- Version Utilities ---
bool semver_is_valid(const char* version_string);
SemVer* semver_increment_major(const SemVer* version);
SemVer* semver_increment_minor(const SemVer* version);
SemVer* semver_increment_patch(const SemVer* version);

// --- Version Range Resolution ---
SemVer* semver_resolve_latest_compatible(const SemVer** versions, size_t count, const VersionConstraint* constraint);
SemVer** semver_filter_versions(const SemVer** versions, size_t count, const VersionConstraint* constraint, size_t* result_count);

#endif // CPM_SEMVER_H