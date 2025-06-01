# CPM Documentation

Welcome to the CPM (C Package Manager) documentation.

## Table of Contents

- [Getting Started](manual/getting-started.md)
- [Installation](manual/installation.md)
- [API Reference](api/README.md)
- [Design Documents](design/README.md)

## Quick Start

```bash
# Install a package
cpm install mylib

# Initialize a new package
cpm init

# Run a script
cpm run build

# Search for packages
cpm search graphics
```

## Architecture

CPM is built on three core technologies:

1. **Q Promises in C** - Asynchronous operation management
2. **PMLL (Persistent Memory Lock Library)** - Race condition prevention and data integrity
3. **PMDK Integration** - Persistent memory support for crash recovery

## Components

- **Core Library** (`lib/core/`) - Q Promise implementation, PMLL, package management
- **CLI Interface** (`lib/cli/`) - Command-line interface
- **Commands** (`lib/commands/`) - Individual command implementations
- **Node Compatibility** (`lib/node_compat/`) - Node.js-style utilities

## License

CPM is licensed under The Artistic License 2.0. See [LICENSE](../LICENSE) for details.