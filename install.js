#!/usr/bin/env node

const { execSync } = require('child_process');
const fs = require('fs');
const path = require('path');

console.log('Installing CPM - C Package Manager...');

try {
    // Check if make is available
    execSync('make --version', { stdio: 'ignore' });
} catch (error) {
    console.error('Error: make is not installed. Please install build tools first.');
    console.error('On Ubuntu/Debian: sudo apt-get install build-essential');
    console.error('On CentOS/RHEL: sudo yum groupinstall "Development Tools"');
    console.error('On macOS: xcode-select --install');
    process.exit(1);
}

// Check for required libraries
const requiredLibs = ['libcurl', 'libjson-c'];
const missingLibs = [];

requiredLibs.forEach(lib => {
    try {
        execSync(`pkg-config --exists ${lib.replace('lib', '')}`, { stdio: 'ignore' });
    } catch (error) {
        missingLibs.push(lib);
    }
});

if (missingLibs.length > 0) {
    console.error('Error: Missing required libraries:', missingLibs.join(', '));
    console.error('On Ubuntu/Debian: sudo apt-get install libcurl4-openssl-dev libjson-c-dev');
    console.error('On CentOS/RHEL: sudo yum install libcurl-devel json-c-devel');
    console.error('On macOS: brew install curl json-c');
    process.exit(1);
}

try {
    console.log('Building CPM binary...');
    execSync('make clean && make', { 
        stdio: 'inherit',
        cwd: __dirname 
    });
    
    console.log('✓ CPM installation completed successfully!');
    console.log('You can now use: cpm --help');
    
} catch (error) {
    console.error('Build failed:', error.message);
    process.exit(1);
}
