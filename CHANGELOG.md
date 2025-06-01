# Changelog

All notable changes to CPM (C Package Manager) will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Initial CPM implementation with Q Promises in C
- PMLL (Persistent Memory Lock Library) hardening integration
- Complete directory structure following npm model
- Core promise functionality: promise_create, promise_then, promise_resolve, promise_reject
- Q.defer() pattern implementation
- Q.all() for promise aggregation
- PMLL hardened queue for serialized operations
- CPM CLI with commands: install, publish, search, run, init, help
- Package parsing and dependency resolution
- Build system with CMake and Makefile support
- Comprehensive test suite
- GitHub Actions CI/CD pipeline

### Changed
- N/A (initial release)

### Deprecated
- N/A (initial release)

### Removed
- N/A (initial release)

### Fixed
- N/A (initial release)

### Security
- PMLL hardening prevents race conditions in file operations
- Package signature verification
- Checksum validation for downloaded packages

## [0.1.0-alpha] - 2024-01-01

### Added
- Initial alpha release
- Basic Q Promise implementation
- PMLL integration foundation
- CPM CLI skeleton
- Package management structure
- Build system setup

---

## Version History

- 0.1.0-alpha: Initial alpha release with core Q Promise functionality