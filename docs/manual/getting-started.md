# Getting Started with CPM

## What is CPM?

CPM (C Package Manager) is a package manager for C libraries and applications. It brings modern package management concepts to the C ecosystem, including:

- **Q Promises** for asynchronous operations
- **PMLL hardening** for race condition prevention
- **Dependency resolution** similar to npm
- **Build system integration**

## Basic Commands

### Installing Packages

```bash
# Install a single package
cpm install mylib

# Install multiple packages
cpm install mylib1 mylib2 mylib3

# Install a specific version
cpm install mylib@1.2.0
```

### Package Information

```bash
# Search for packages
cpm search graphics

# View package information
cpm view mylib

# List installed packages
cpm ls
```

### Project Management

```bash
# Initialize a new project
cpm init

# Run project scripts
cpm run build
cpm run test

# Update dependencies
cpm update
```

## Package Specification

CPM uses `cpm_package.spec` files (JSON format) for package metadata:

```json
{
  "name": "mylib",
  "version": "1.0.0",
  "description": "My C library",
  "author": "Your Name <your.email@example.com>",
  "license": "MIT",
  "dependencies": {
    "otherlib": ">=2.0.0"
  },
  "scripts": {
    "build": "make",
    "test": "make test",
    "install": "make install"
  }
}
```

## Next Steps

- Read the [Installation Guide](installation.md)
- Explore the [API Reference](../api/README.md)
- Learn about [Q Promises](../design/q-promises.md)
- Understand [PMLL Hardening](../design/pmll-hardening.md)