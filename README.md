# CPM - C Package Manager

A hardened C implementation of NPM with Q promises and PMLL (Package Manager Linked List) support.

## Features

- **🚀 Full NPM CLI Compatibility**: All standard npm commands implemented in C
- **⚡ Q Promises**: Asynchronous operations with promise-like syntax in C
- **🔗 PMLL**: Advanced package management with linked list data structures
- **🔒 Hardened Security**: Memory-safe C implementation with robust error handling
- **🔄 Bidirectional NPM Support**: `npm install cpm` and `cpm install npm` both work
- **🎯 Performance**: Native C speed for package operations

## Installation

### Via NPM (Recommended)
```bash
npm install -g cpm
```

### From Source
```bash
git clone https://github.com/cpm/cpm.git
cd cpm
npm install
npm run build
```

## System Requirements

- GCC compiler
- libcurl development libraries
- libjson-c development libraries
- pthread support

### Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get install build-essential libcurl4-openssl-dev libjson-c-dev
```

**CentOS/RHEL:**
```bash
sudo yum groupinstall "Development Tools"
sudo yum install libcurl-devel json-c-devel
```

**macOS:**
```bash
xcode-select --install
brew install curl json-c
```

## Usage

CPM provides a complete npm-compatible CLI:

### Package Installation
```bash
cpm install express           # Install latest version
cpm install lodash@4.17.21    # Install specific version
cpm install --save-dev jest   # Install as dev dependency
cpm install                   # Install from package.json
```

### Package Management
```bash
cpm uninstall package-name    # Remove package
cpm update                    # Update all packages
cpm update package-name       # Update specific package
cpm list                      # List installed packages
```

### Package Information
```bash
cpm info package-name         # Show package details
cpm audit                     # Audit for vulnerabilities
```

### Project Management
```bash
cpm init                      # Initialize new package.json
```

### Options
```bash
--save, -S          Save to dependencies
--save-dev, -D      Save to devDependencies  
--global, -g        Install globally
--verbose, -v       Verbose output
--dry-run          Show what would be done
--help, -h         Show help
```

## Q Promises in C

CPM includes a comprehensive Q Promise implementation for asynchronous operations:

```c
#include "cpm.h"

QPromise* promise = q_promise_new();

// Async operation
q_promise_then(promise, on_success);
q_promise_catch(promise, on_error);

// Resolve/reject
q_promise_resolve(promise, result_data);
q_promise_reject(promise, "Error message");

// Await-style
AwaitResult result = q_await(promise);
if (result.error) {
    // Handle error
} else {
    // Use result.result
}
```

## PMLL (Package Manager Linked List)

Advanced package management with linked list operations:

```c
PMLL* list = pmll_new();

Package package = {
    .name = "express",
    .version = "4.18.0",
    .description = "Fast, minimalist web framework"
};

pmll_add_package(list, &package);
Package* found = pmll_find_package(list, "express");
pmll_remove_package(list, "express");
pmll_free(list);
```

## Bidirectional NPM Compatibility

CPM maintains full bidirectional compatibility with NPM:

```bash
# These all work seamlessly
npm install cpm
cpm install npm
cpm install express
npm install express
```

## API Reference

### Core Functions
- `cpm_init()` - Initialize CPM context
- `cpm_install()` - Install packages
- `cpm_uninstall()` - Remove packages
- `cpm_update()` - Update packages
- `cpm_list()` - List packages
- `cpm_info()` - Package information
- `cpm_audit()` - Security audit

### Q Promise Functions
- `q_promise_new()` - Create new promise
- `q_promise_resolve()` - Resolve promise
- `q_promise_reject()` - Reject promise
- `q_promise_then()` - Success callback
- `q_promise_catch()` - Error callback
- `q_await()` - Await promise result

### PMLL Functions
- `pmll_new()` - Create new list
- `pmll_add_package()` - Add package
- `pmll_find_package()` - Find package
- `pmll_remove_package()` - Remove package
- `pmll_check_conflicts()` - Check version conflicts

## Architecture

CPM is built with a modular architecture:

- **Core Engine**: Package management logic in C
- **Q Promises**: Asynchronous operation handling
- **PMLL**: Linked list-based package storage
- **HTTP Client**: libcurl-based registry communication
- **JSON Parser**: libjson-c for metadata handling
- **NPM Wrapper**: Node.js compatibility layer

## Contributing

1. Fork the repository
2. Create your feature branch
3. Make your changes
4. Add tests
5. Submit a pull request

## License

MIT License - see LICENSE file for details

## Security

CPM is designed with security in mind:
- Memory-safe operations
- Input validation
- Secure HTTP communications
- Buffer overflow protection

Report security issues to security@cpm.dev

## Performance

CPM provides significant performance improvements over NPM:
- Native C speed
- Optimized memory usage
- Efficient dependency resolution
- Fast I/O operations

## Support

- GitHub Issues: https://github.com/cpm/cpm/issues
- Documentation: https://cpm.dev/docs
- Community: https://discord.gg/cpm

---

**CPM - Package Management, Reimagined in C** 🚀
