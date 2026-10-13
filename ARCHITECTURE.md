# Architecture and engineering plan

## Current architecture

```text
Browser UI (web/index.html, styles.css, app.js)
  ├── Terminal input, history, suggestions
  ├── Directory tree and explorer
  ├── Command guide and C source excerpts
  ├── Guided missions and learning feedback
  └── OS Concept Lab: metrics, traversal, quiz
           │
           ▼
Browser-side JavaScript engine (web/engine.js)
  ├── In-memory directory/file nodes
  ├── Command parsing and path resolution
  ├── Mutations, snapshots for undo/redo
  └── Snapshot export/import for local storage

Original native C implementation (mini_fs.c)
  └── Separate executable, compiled and run by GCC
```

The browser engine is separate from the C program. This is disclosed in the UI and README. Browser visualizations and timings must not be described as native C runtime instrumentation.

## Phase 3 quality controls

- `npm test`: syntax, engine behavior, Phase 2 UI bindings, static asset references, unique HTML IDs, and additional edge cases.
- `.github/workflows/ci.yml`: runs Node tests, compiles the native C source, and performs a basic native command smoke test.
- `.github/workflows/pages.yml`: deploys the static `web/` directory to GitHub Pages when configured.
- `Dockerfile`: optional Nginx container for the static browser app.
- `npm run benchmark`: informational measurements of the JavaScript browser engine; not a CI gate.

## Proposed WebAssembly integration

1. Audit platform-specific and persistent I/O in `mini_fs.c`.
2. Separate initialization and command execution from `main()` so the C filesystem tree remains the source of truth.
3. Create a callable API, for example `minifs_init`, `minifs_execute(command)`, and `minifs_export_tree_json`.
4. Capture command output without depending on an interactive `fgets(stdin)` loop.
5. Adapt autosave/save/load to Emscripten's virtual filesystem or disable unsupported persistence in browser mode.
6. Compile with Emscripten and expose the API using exported C functions or a small JavaScript bridge.
7. Add command parity tests against the native C implementation before changing the browser engine.
8. Switch the UI to WebAssembly only after browser smoke tests and parity tests pass.

## Correctness and safety principles

- Never claim WebAssembly execution until browser interactions demonstrably call the compiled C functions.
- Validate command inputs in the engine; UI validation alone is not a security boundary.
- Keep destructive operations confirmable and make the virtual nature of the filesystem clear.
- Test duplicate names, invalid paths, recursive operations, undo/redo, quota boundaries, snapshot import, and UTF-8 content accounting.
- Complexity descriptions are theoretical and implementation-dependent, not measured performance claims.
- The local Node server is a development helper, not intended as a public production server.
