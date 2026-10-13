const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const root = path.join(__dirname, '..');
let checks = 0;
function check(condition, message) { checks++; assert.ok(condition, message); }
function equal(actual, expected, message) { checks++; assert.equal(actual, expected, message); }

// Static asset integrity: local script/style references must resolve and HTML IDs must be unique.
const html = fs.readFileSync(path.join(root, 'web/index.html'), 'utf8');
const ids = [...html.matchAll(/\bid=["']([^"']+)["']/g)].map(m => m[1]);
equal(ids.length, new Set(ids).size, 'HTML IDs must be unique');
for (const match of html.matchAll(/<(?:script|link)\b[^>]*(?:src|href)=["']([^"']+)["'][^>]*>/gi)) {
  const asset = match[1];
  if (/^(?:https?:|data:|#|\/\/)/i.test(asset)) continue;
  const localPath = asset.split(/[?#]/)[0];
  check(fs.existsSync(path.join(root, 'web', localPath)), `Referenced local asset must exist: ${asset}`);
}
for (const required of ['web/index.html', 'web/app.js', 'web/engine.js', 'web/styles.css', 'mini_fs.c', 'ARCHITECTURE.md', 'README.md']) {
  check(fs.existsSync(path.join(root, required)), `Required project file exists: ${required}`);
}

// Load the engine in an isolated VM and test additional boundary cases.
const context = { window: {}, TextEncoder, console };
vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(root, 'web/engine.js'), 'utf8'), context);
const engine = context.window.MiniFSEngine;
function run(command) { const result = engine.execute(command); check(result.kind !== 'error', `${command} succeeds: ${result.output.join('\n')}`); return result; }
function fail(command, message) { const result = engine.execute(command); equal(result.kind, 'error', `${command} should be rejected`); if (message) check(result.output.join('\n').toLowerCase().includes(message.toLowerCase()), `${command} explains failure`); }

engine.reset();
run('mkdir -p /quality/a/b');
run('touch /quality/a/b/unicode.txt');
run('write /quality/a/b/unicode.txt "héllo 🌍"');
equal(engine.findPath(engine.getState().root, '/quality/a/b/unicode.txt').content, 'héllo 🌍', 'quoted UTF-8 content round-trips');
const utf8Bytes = new TextEncoder().encode('héllo 🌍').length;
equal(engine.getState().stats.bytes, utf8Bytes + new TextEncoder().encode(engine.findPath(engine.getState().root, '/docs/getting-started.md').content).length + new TextEncoder().encode(engine.findPath(engine.getState().root, '/src/main.c').content).length + new TextEncoder().encode(engine.findPath(engine.getState().root, '/notes.txt').content).length + new TextEncoder().encode(engine.findPath(engine.getState().root, '/data/readme.txt').content).length, 'content size uses UTF-8 bytes');
fail('mkdir /quality', 'already exists');
fail('touch /quality/a/b/unicode.txt', 'already exists');
fail('write /quality/a/b invalid', 'directory');
fail('cd /quality/a/b/unicode.txt', 'not a directory');
fail('mv /quality /quality/a/inside', 'inside itself');
fail('rename /quality/a/b/unicode.txt invalid/name', 'invalid');
fail('mkdir /' + 'x'.repeat(64), 'invalid name');
fail('write /quality/a/b/unicode.txt "unclosed', 'unclosed quote');
run('quota ' + engine.getState().stats.bytes);
fail('append /quality/a/b/unicode.txt x', 'quota exceeded');
run('chmod 600 /quality/a/b/unicode.txt');
run('undo');
equal(engine.findPath(engine.getState().root, '/quality/a/b/unicode.txt').permissions, '644', 'undo restores permission');
run('redo');
equal(engine.findPath(engine.getState().root, '/quality/a/b/unicode.txt').permissions, '600', 'redo reapplies permission');
run('fsck');
const snapshot = engine.exportTree();
engine.reset();
engine.importTree(snapshot);
equal(engine.findPath(engine.getState().root, '/quality/a/b/unicode.txt').content, 'héllo 🌍', 'export/import preserves UTF-8 content');
const beforeInvalidImport = engine.exportTree();
let invalidImportRejected = false;
try { engine.importTree({ type: 'file', name: 'bad', children: [] }); } catch { invalidImportRejected = true; }
check(invalidImportRejected, 'invalid snapshot root is rejected');
equal(engine.findPath(engine.getState().root, '/quality/a/b/unicode.txt').content, 'héllo 🌍', 'invalid root import does not replace existing tree');
engine.reset();
fail('undo', 'nothing to undo');
fail('redo', 'nothing to redo');
console.log(`PASS: ${checks} Phase 3 quality checks for asset references, unique IDs, UTF-8 accounting, path/command boundaries, quota, undo/redo, snapshot validation, and reset.`);
