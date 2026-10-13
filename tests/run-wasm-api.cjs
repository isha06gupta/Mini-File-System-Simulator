'use strict';
const { spawnSync } = require('node:child_process');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const root = path.resolve(__dirname, '..');
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'minifs-c-api-'));
const executable = path.join(tmp, process.platform === 'win32' ? 'wasm-api-smoke.exe' : 'wasm-api-smoke');
function run(command, args, cwd = root) {
  const result = spawnSync(command, args, { cwd, encoding: 'utf8' });
  if (result.stdout) process.stdout.write(result.stdout);
  if (result.stderr) process.stderr.write(result.stderr);
  if (result.error) throw result.error;
  if (result.status !== 0) process.exit(result.status || 1);
}
try {
  run('gcc', ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', 'tests/wasm-api-smoke.c', 'wasm/minifs_wasm.c', '-o', executable]);
  run(executable, [], tmp);
} finally {
  fs.rmSync(tmp, { recursive: true, force: true });
}
