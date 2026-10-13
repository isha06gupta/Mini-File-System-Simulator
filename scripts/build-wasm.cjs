'use strict';
const { spawnSync } = require('node:child_process');
const path = require('node:path');
const root = path.resolve(__dirname, '..');
const windows = process.platform === 'win32';
const command = windows ? 'powershell.exe' : 'sh';
const args = windows
  ? ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', path.join(root, 'scripts', 'build-wasm.ps1')]
  : [path.join(root, 'scripts', 'build-wasm.sh')];
const result = spawnSync(command, args, { cwd: root, stdio: 'inherit' });
if (result.error) { console.error(result.error.message); process.exit(1); }
process.exit(result.status === null ? 1 : result.status);
