const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { performance } = require('node:perf_hooks');
const root = path.join(__dirname, '..');
const context = { window: {}, TextEncoder, console };
vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(root, 'web/engine.js'), 'utf8'), context);
const engine = context.window.MiniFSEngine;
const sizes = [100, 500, 1000];
console.log('MiniFS browser-engine microbenchmark (informational; not a CI performance gate)');
console.log('Node.js:', process.version);
for (const size of sizes) {
  engine.reset();
  const startBuild = performance.now();
  for (let i = 0; i < size; i++) {
    const result = engine.execute(`touch /bench-${String(i).padStart(5, '0')}.txt`);
    if (result.kind === 'error') throw new Error(result.output.join('\n'));
  }
  const buildMs = performance.now() - startBuild;
  const startTree = performance.now();
  engine.execute('tree');
  const treeMs = performance.now() - startTree;
  const startCount = performance.now();
  engine.execute('count');
  const countMs = performance.now() - startCount;
  console.log(`${size} added files | build ${buildMs.toFixed(2)} ms | tree ${treeMs.toFixed(2)} ms | count ${countMs.toFixed(2)} ms`);
}
console.log('Note: timings vary by machine/runtime and are not native C or WebAssembly benchmarks.');
