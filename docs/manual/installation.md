# Installing CPM

## Prerequisites

CPM requires the following dependencies:

### Required
- **C11 compliant compiler** (GCC 7+, Clang 6+)
- **CMake 3.16+** or **Make**
- **PMDK (Persistent Memory Development Kit)**
- **cJSON library**
- **pthread library**

### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install build-essential cmake
sudo apt-get install libpmem-dev libpmemobj-dev
sudo apt-get install libcjson-dev
```

### CentOS/RHEL/Fedora

```bash
# Fedora
sudo dnf install gcc cmake make
sudo dnf install pmdk-devel libpmem-devel
sudo dnf install cjson-devel

# CentOS/RHEL (with EPEL)
sudo yum install epel-release
sudo yum install gcc cmake3 make
sudo yum install pmdk-devel
sudo yum install cjson-devel
```

### macOS

```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake
brew install pmdk
brew install cjson
```

## Building from Source

### Using CMake (Recommended)

```bash
git clone https://github.com/Qchains/CPM-cli.git
cd CPM-cli

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
sudo make install
```

### Using Make

```bash
git clone https://github.com/Qchains/CPM-cli.git
cd CPM-cli

make release
sudo make install
```

## Verification

After installation, verify CPM is working:

```bash
cpm --version
cpm --help
```

## Configuration

Create a global configuration file:

```bash
mkdir -p ~/.cpm
cat > ~/.cpm/config << EOF
registry=https://registry.cpm.example.org
loglevel=3
pmll-enabled=true
EOF
```

## Troubleshooting

### PMDK Not Found

If PMDK is not found during build:

```bash
# Check if PMDK is installed
pkg-config --exists libpmem libpmemobj
echo $?  # Should return 0

# If not found, install from source
git clone https://github.com/pmem/pmdk.git
cd pmdk
make -j$(nproc)
sudo make install
```

### Permission Issues

If you get permission errors during installation:

```bash
# Install to user directory instead
cmake -DCMAKE_INSTALL_PREFIX=$HOME/.local ..
make install

# Add to PATH
echo 'export PATH=$HOME/.local/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

### Build Errors

Common build issues:

1. **C11 not supported**: Update your compiler
2. **Missing headers**: Install development packages
3. **Linking errors**: Check library paths with `ldconfig -p`