#!/usr/bin/env node

const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');

// Path to the compiled CPM binary
const binaryName = process.platform === 'win32' ? 'cpm.exe' : 'cpm';
const binaryPath = path.join(__dirname, 'bin', binaryName);

// Check if binary exists
if (!fs.existsSync(binaryPath)) {
    console.error('CPM binary not found. Please run "npm run build" first.');
    process.exit(1);
}

// Forward all arguments to the CPM binary
const args = process.argv.slice(2);
const child = spawn(binaryPath, args, {
    stdio: 'inherit',
    cwd: process.cwd()
});

child.on('close', (code) => {
    process.exit(code);
});

child.on('error', (err) => {
    console.error('Failed to start CPM:', err.message);
    process.exit(1);
});
