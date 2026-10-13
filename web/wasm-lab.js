(function () {
  'use strict';
  const $ = (id) => document.getElementById(id);
  const status = $('runtimeStatus');
  const output = $('output');
  const input = $('commandInput');
  const runButton = $('runBtn');
  const resetButton = $('resetBtn');
  let moduleInstance = null;

  function setStatus(message, kind) {
    status.textContent = message;
    status.className = 'status status-' + kind;
  }
  function line(text, kind) {
    const el = document.createElement('div');
    el.className = 'line ' + (kind || 'output');
    el.textContent = text;
    output.appendChild(el);
    output.scrollTop = output.scrollHeight;
  }
  function enable(enabled) {
    input.disabled = !enabled;
    runButton.disabled = !enabled;
    resetButton.disabled = !enabled;
    document.querySelectorAll('[data-command]').forEach((button) => { button.disabled = !enabled; });
  }
  function drawTree(node, parent) {
    const row = document.createElement('div');
    row.className = 'tree-row ' + (node.type === 'file' ? 'tree-file' : 'tree-dir');
    const icon = document.createElement('span');
    icon.textContent = node.type === 'dir' ? '▰' : '▤';
    const name = document.createElement('span');
    name.className = 'tree-name';
    name.textContent = node.name + (node.type === 'dir' && node.path !== '/' ? '/' : '');
    row.append(icon, name);
    if (node.type === 'file') {
      const meta = document.createElement('span');
      meta.className = 'tree-meta';
      meta.textContent = node.size + ' B · ' + node.permissions.toString(8);
      row.appendChild(meta);
    }
    parent.appendChild(row);
    (node.children || []).slice().sort((a, b) => a.type === b.type ? a.name.localeCompare(b.name) : (a.type === 'dir' ? -1 : 1)).forEach((child) => {
      const wrap = document.createElement('div');
      wrap.className = 'tree-node';
      drawTree(child, wrap);
      parent.appendChild(wrap);
    });
  }
  function refreshTree() {
    const json = moduleInstance.ccall('minifs_export_tree_json', 'string', [], []);
    const root = JSON.parse(json);
    const host = $('tree');
    host.replaceChildren();
    drawTree(root, host);
    $('cwd').textContent = 'Current path: ' + moduleInstance.ccall('minifs_get_cwd', 'string', [], []);
  }
  function execute(command) {
    const value = command.trim();
    if (!value || !moduleInstance) return;
    line('/› ' + value, 'command');
    const response = moduleInstance.ccall('minifs_execute', 'string', ['string'], [value]);
    if (response.trim()) response.replace(/\n$/, '').split('\n').forEach((text) => line(text, /error|failed|invalid|unknown/i.test(text) ? 'error' : 'output'));
    refreshTree();
  }
  function loadWasmScript() {
    return fetch('minifs-c.js', { method: 'HEAD', cache: 'no-store' }).then((response) => {
      if (!response.ok) throw new Error('WebAssembly build artifacts are not present yet.');
      return new Promise((resolve, reject) => {
        const script = document.createElement('script');
        script.src = 'minifs-c.js';
        script.onload = resolve;
        script.onerror = () => reject(new Error('Could not load minifs-c.js.'));
        document.head.appendChild(script);
      });
    });
  }
  function boot() {
    loadWasmScript().then(() => {
      if (typeof window.MiniFSCModule !== 'function') throw new Error('The compiled module did not expose MiniFSCModule.');
      return window.MiniFSCModule({ print: (text) => line(text, 'output'), printErr: (text) => line(text, 'error') });
    })
      .then((instance) => {
        moduleInstance = instance;
        moduleInstance.ccall('minifs_init', null, [], []);
        enable(true);
        setStatus('C engine loaded in WebAssembly. Commands execute against the C-managed filesystem.', 'ok');
        output.replaceChildren();
        line('MiniFS C engine ready. Type help to see supported commands.', 'muted');
        refreshTree();
        input.focus();
      })
      .catch((error) => {
        const missingBuild = /not present yet/.test(String(error && error.message));
        setStatus(missingBuild ? 'WebAssembly build not found. The main Learning Lab still works. Follow wasm/README.md, build the module, then reload this page.' : 'Could not initialize the WebAssembly module. Check the build output and browser console.', missingBuild ? 'warn' : 'error');
        line(String(error && error.message ? error.message : error), missingBuild ? 'muted' : 'error');
        enable(false);
      });
  }
  $('commandForm').addEventListener('submit', (event) => {
    event.preventDefault();
    const value = input.value;
    input.value = '';
    execute(value);
    input.focus();
  });
  $('clearBtn').addEventListener('click', () => { output.replaceChildren(); });
  resetButton.addEventListener('click', () => {
    if (!moduleInstance || !window.confirm('Reset the C-managed virtual filesystem?')) return;
    moduleInstance.ccall('minifs_init', null, [], []);
    output.replaceChildren();
    line('C filesystem reset.', 'muted');
    refreshTree();
  });
  document.querySelectorAll('[data-command]').forEach((button) => button.addEventListener('click', () => execute(button.dataset.command)));
  boot();
})();
