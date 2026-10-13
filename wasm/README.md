# C / WebAssembly engine lab

The main `web/index.html` learning interface continues to use the JavaScript teaching engine. The separate `web/wasm-lab.html` page is designed to call the original C implementation compiled to WebAssembly. It intentionally stays disabled until `web/minifs-c.js` and `web/minifs-c.wasm` have been built; it never silently falls back to JavaScript while claiming to run C.

## What the adapter does

- Includes the existing `mini_fs.c` implementation in `wasm/minifs_wasm.c`.
- Redirects the engine's `printf` output to a bounded output buffer.
- Exposes `minifs_init`, `minifs_execute`, `minifs_export_tree_json`, `minifs_get_cwd`, and `minifs_free`.
- Exports a JSON representation of the C-managed `Node` tree for the browser view.
- Reuses C operations for commands rather than reimplementing filesystem behavior in JavaScript.

## Build with Emscripten

Install Emscripten from the official guide and activate its environment: <https://emscripten.org/docs/getting_started/downloads.html>.

From the project root:

### Linux / macOS / MSYS2 shell

```sh
npm run build:wasm
```

### Windows PowerShell (after `emcc` is available on PATH)

```powershell
.\scripts\build-wasm.ps1
```

The build generates `web/minifs-c.js` and `web/minifs-c.wasm`. These generated artifacts are intentionally excluded from source control by `.gitignore`; build them locally or in CI. Start the static server with `npm run serve`, then open `http://127.0.0.1:8000/wasm-lab.html`.

## Native API smoke test

The same adapter is compiled and exercised as a native executable in the test suite, which checks initialization, directory/file operations, output capture, current-directory reporting, JSON tree export, and `fsck` integrity. This is a useful adapter test, but it is **not a substitute for running the Emscripten build in a real browser**.

## Browser limitations

- Filesystem persistence is in-memory unless a user explicitly uses `save`/`load` in the virtual filesystem; this adapter does not auto-restore the native CLI's host autosave file.
- `save` and `load` operate inside Emscripten's virtual filesystem, not the user's real computer filesystem.
- This API adapter has no native OS access and does not execute shell commands.
- Large trees can exceed the output buffer; the current learning demo is intended for small teaching sandboxes.
- Step-through stages and educational explanations in the main app are teaching aids; they are not instruction-by-instruction instrumentation of the C runtime.
