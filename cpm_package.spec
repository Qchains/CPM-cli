{
  "name": "cpm",
  "version": "0.1.0-alpha",
  "description": "C Package Manager with Q Promises and PMLL hardening",
  "author": "Dr. Q Josef Kurk Edwards <spgaga24@gmail.com>",
  "license": "Artistic-2.0",
  "homepage": "https://github.com/Qchains/CPM-cli",
  "repository": "https://github.com/Qchains/CPM-cli.git",
  "bugs": "https://github.com/Qchains/CPM-cli/issues",
  
  "keywords": [
    "package-manager",
    "c",
    "promises",
    "pmll",
    "persistent-memory",
    "build-system"
  ],
  
  "category": "development-tools",
  
  "dependencies": {
    "libcjson": ">=1.7.0",
    "libpmem": ">=1.10.0",
    "libpmemobj": ">=1.10.0"
  },
  
  "devDependencies": {
    "cmake": ">=3.16",
    "cppcheck": ">=2.0",
    "clang-tidy": ">=10.0"
  },
  
  "scripts": {
    "build": "mkdir -p build && cd build && cmake .. && make",
    "test": "cd build && ctest --output-on-failure",
    "install": "cd build && sudo make install",
    "clean": "rm -rf build bin/cpm bin/cpx",
    "format": "clang-format -i lib/**/*.c include/*.h",
    "lint": "cppcheck --enable=all lib/ include/",
    "docs": "doxygen docs/Doxyfile"
  },
  
  "buildConfig": {
    "buildSystem": "cmake",
    "minCStandard": "c11",
    "includeDirs": ["include", "/usr/include/libpmem"],
    "libraryDirs": ["/usr/lib", "/usr/local/lib"],
    "libraries": ["pmem", "pmemobj", "cjson", "pthread"]
  },
  
  "cpmConfig": {
    "minCpmVersion": "0.1.0",
    "includePath": "include",
    "libPath": "lib",
    "binPath": "bin",
    "docPath": "docs",
    "isBinaryPackage": false,
    "isHeaderOnly": false,
    "requiresBuild": true,
    "isSystemPackage": false
  }
}