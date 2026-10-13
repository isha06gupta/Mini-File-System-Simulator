const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const context = { window: {}, TextEncoder, console };
vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(__dirname, '../web/engine.js'), 'utf8'), context);
const e = context.window.MiniFSEngine;
let assertions = 0;
function check(value, message) { assertions++; assert.ok(value, message); }
function run(cmd) { const r = e.execute(cmd); check(r.kind !== 'error', `${cmd}: ${r.output.join('\n')}`); return r; }
function fails(cmd, fragment) { const r=e.execute(cmd); check(r.kind==='error', `${cmd} should fail`); if(fragment) check(r.output.join('\n').toLowerCase().includes(fragment.toLowerCase()), `${cmd} error should mention ${fragment}`); }

run('mkdir qa'); run('touch qa/a.txt'); run('write qa/a.txt hello world');
check(e.findPath(e.getState().root,'/qa/a.txt').content==='hello world','write content');
run('append qa/a.txt !'); check(e.findPath(e.getState().root,'/qa/a.txt').content==='hello world!','append content');
run('mkdir -p qa/sub/deep'); run('cp qa/a.txt qa/sub/copy.txt');
check(e.findPath(e.getState().root,'/qa/sub/copy.txt').content==='hello world!','deep-copy file contents');
run('rename qa/sub/copy.txt copied.txt'); check(!!e.findPath(e.getState().root,'/qa/sub/copied.txt'),'rename');
run('chmod 600 qa/a.txt'); check(e.findPath(e.getState().root,'/qa/a.txt').permissions==='600','chmod');
check(run('find *.txt').output.some(x=>x.endsWith('a.txt')),'wildcard find');
run('undo'); check(e.findPath(e.getState().root,'/qa/a.txt').permissions==='644','undo restores permission');
run('redo'); check(e.findPath(e.getState().root,'/qa/a.txt').permissions==='600','redo restores permission');
check(run('fsck').output[0].includes('passed'),'fsck clean tree'); check(run('tree').output.some(x=>x.includes('deep')),'tree traversal');
run('cd qa'); check(e.getState().path==='/qa','cd path'); run('cd ..'); check(e.getState().path==='/','cd parent');
run('write /qa/a.txt "quoted text here"'); check(e.findPath(e.getState().root,'/qa/a.txt').content==='quoted text here','quoted text parsing');
run('quota 1000'); run('quotainfo');
const exported=e.exportTree(); e.reset(); e.importTree(exported); check(!!e.findPath(e.getState().root,'/qa/a.txt'),'snapshot export/import');
fails('mkdir qa','already exists'); fails('read qa','directory'); fails('touch missing/child.txt','parent directory'); fails('rmdir qa','not empty'); fails('chmod abc qa/a.txt','octal'); fails('quota -1','non-negative'); fails('not-a-command','unknown command'); fails('read /missing.txt','path not found');
run('rm -r qa'); check(!e.findPath(e.getState().root,'/qa'),'recursive remove');
run('undo'); check(!!e.findPath(e.getState().root,'/qa/a.txt'),'undo restores removed tree');
run('reset'); check(!!e.findPath(e.getState().root,'/docs/getting-started.md'),'reset sample workspace');
console.log(`PASS: ${assertions} assertions covering core operations, navigation, quoting, permissions, search, quota, snapshots, invalid input, recursive deletion, and reset.`);
